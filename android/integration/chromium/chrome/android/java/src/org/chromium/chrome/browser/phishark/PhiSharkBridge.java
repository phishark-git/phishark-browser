// SPDX-License-Identifier: GPL-3.0-only
package org.chromium.chrome.browser.phishark;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.ImageView;
import java.lang.ref.WeakReference;
import java.net.URI;
import java.util.Arrays;
import java.util.WeakHashMap;
import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import io.phishark.browser.security.ApiKeyVault;

/** Browser-process UI and vault access; this class is never installed as a JS interface. */
public final class PhiSharkBridge {
    private static final String BASE = "phishark.api_base";
    private static final String CONSENT = "phishark.deep_consent.v1";
    private static final String VERSION = "phishark.settings_version";
    private static final String[] LABELS = {"Kontrol ediliyor", "Güvenli", "Uyarı",
            "Engellendi", "Kontrol edilemedi", "Kurulum / hizmet hatası"};
    private static final WeakHashMap<WebContents, State> STATES = new WeakHashMap<>();
    private static final WeakHashMap<Activity, PhiSharkBridge> WINDOWS = new WeakHashMap<>();
    private final WeakReference<Activity> activity;
    private final ActivityTabProvider tabs;
    private final View uiHost;
    private final TextView scanIndicator;
    private final Runnable showScanIndicator = this::showScanningIndicator;
    private WebContents indicatorContents;
    private long indicatorGeneration = -1;
    private ActivityTabProvider.ActivityTabTabObserver tabObserver;
    private AlertDialog verdictDialog;
    private WebContents dialogContents;
    private long dialogGeneration = -1;
    private AlertDialog accountDialog;
    private boolean accountDialogConnected;

    private static final class State {
        final int verdict;
        final long generation;
        final String score;
        final String lastSafe;
        final boolean deep;
        final String detail;
        final int urlVerdict;
        boolean warningAccepted;
        boolean deepPending;
        State(int verdict, long generation, String score, String lastSafe, boolean deep, String detail, int urlVerdict) {
            this.verdict = verdict; this.generation = generation;
            this.score = score; this.lastSafe = lastSafe; this.deep = deep;
            this.detail = detail;
            this.urlVerdict = urlVerdict;
        }
    }

    private PhiSharkBridge(Activity owner, ActivityTabProvider provider) {
        activity = new WeakReference<>(owner); tabs = provider;
        uiHost = owner.findViewById(android.R.id.content);
        scanIndicator = new TextView(owner);
        scanIndicator.setText("PhiShark kontrol ediyor · Bekleyin");
        scanIndicator.setTextSize(12);
        scanIndicator.setTextColor(0xFFE1F3F5);
        scanIndicator.setPadding(dp(12), dp(7), dp(12), dp(7));
        scanIndicator.setGravity(Gravity.CENTER);
        scanIndicator.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        GradientDrawable background = new GradientDrawable();
        background.setColor(0xF0092330);
        background.setCornerRadius(dp(16));
        background.setStroke(dp(1), 0xFF237789);
        scanIndicator.setBackground(background);
        scanIndicator.setVisibility(View.GONE);
        FrameLayout.LayoutParams layout = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        layout.bottomMargin = dp(32);
        layout.leftMargin = layout.rightMargin = dp(16);
        ((FrameLayout) uiHost).addView(scanIndicator, layout);
        // The activity owns the observer and destroys it with its tab provider.
        tabObserver = new ActivityTabProvider.ActivityTabTabObserver(tabs) {
            @Override protected void onObservingDifferentTab(Tab tab) { refresh(); }
        };
        refresh();
        BrowserAccount.setListener(() -> {
            for (PhiSharkBridge window : WINDOWS.values()) window.accountChanged();
        });
        BrowserAccount.credential();
        if (!prefs().getBoolean("phishark.onboarded.v1", false)) uiHost.post(() -> showAccount(true));
    }

    public static void install(Activity owner, ActivityTabProvider tabs) {
        if (!WINDOWS.containsKey(owner)) WINDOWS.put(owner, new PhiSharkBridge(owner, tabs));
    }

    public static void uninstall(Activity owner) {
        PhiSharkBridge bridge = WINDOWS.remove(owner);
        if (bridge == null) return;
        bridge.tabObserver.destroy();
        bridge.uiHost.removeCallbacks(bridge.showScanIndicator);
        if (bridge.scanIndicator.getParent() instanceof ViewGroup) {
            ((ViewGroup) bridge.scanIndicator.getParent()).removeView(bridge.scanIndicator);
        }
        if (bridge.verdictDialog != null) bridge.verdictDialog.dismiss();
        if (bridge.accountDialog != null) bridge.accountDialog.dismiss();
        bridge.dialogContents = null;
    }

    private static SharedPreferences prefs() { return ContextUtils.getAppSharedPreferences(); }
    private static ApiKeyVault vault() { return new ApiKeyVault(ContextUtils.getApplicationContext()); }

    @CalledByNative private static String getApiBase() { return BrowserAccount.enabled() ? BrowserAccount.API : prefs().getString(BASE, BrowserAccount.API); }
    @CalledByNative private static byte[] getApiKey() {
        if (BrowserAccount.enabled()) return BrowserAccount.credential();
        try { return vault().load(); } catch (Exception ignored) { return null; }
    }
    @CalledByNative private static boolean usesAccount() { return BrowserAccount.enabled(); }
    @CalledByNative private static boolean isAccountRefreshing() { return BrowserAccount.isRefreshing(); }
    @CalledByNative private static void refreshAccount() { BrowserAccount.refresh(true); }
    @CalledByNative private static boolean hasDeepConsent() { return prefs().getBoolean(CONSENT, false); }
    @CalledByNative private static long getSettingsVersion() { return prefs().getLong(VERSION, 0); }

    @CalledByNative private static void updateState(@JniType("content::WebContents*") WebContents contents, int verdict,
            long generation, String score, String lastSafe, boolean deep, String detail, int urlVerdict) {
        if (verdict < 0 || verdict >= LABELS.length || contents == null) return;
        State previous = STATES.get(contents);
        if (previous != null && generation < previous.generation) return;
        STATES.put(contents, new State(verdict, generation, score, lastSafe, deep, detail, urlVerdict));
        for (PhiSharkBridge window : WINDOWS.values()) window.refresh();
    }

    private WebContents current() {
        Tab tab = tabs.get(); return tab == null ? null : tab.getWebContents();
    }

    @CalledByNative private static void setDeepPending(
            @JniType("content::WebContents*") WebContents contents, long generation) {
        State state = STATES.get(contents);
        if (state == null || state.generation != generation || state.verdict == 3) return;
        state.deepPending = true;
        for (PhiSharkBridge window : WINDOWS.values()) window.refresh();
    }

    private static String statusLabel(State state) {
        if (state == null) return "Sayfa açın";
        if (state.deepPending) return (state.verdict == 2 ? "Uyarı · " : "") + "Derin analiz sürüyor";
        if (state.verdict == 1 && !state.deep) return "URL kontrolü: düşük risk";
        // A URL verdict is scoped context. It never overrides a deep block,
        // warning or service error, nor turns incomplete capture into safe.
        if (state.verdict == 4 && state.urlVerdict == 1) return "URL düşük risk · Kısmi kontrol";
        if (state.verdict == 4 && state.urlVerdict == 2) return "URL uyarısı · Kısmi kontrol";
        return LABELS[state.verdict];
    }

    private void refresh() {
        WebContents contents = current(); State state = STATES.get(contents);
        refreshScanningIndicator(contents, state);
        // Completed results stay quiet; diagnostics remain in the app menu.
        if (verdictDialog != null && (contents != dialogContents || state == null
                || state.generation != dialogGeneration)) {
            verdictDialog.dismiss(); verdictDialog = null;
        }
        if (state != null && (state.verdict == 3
                || state.verdict == 2 && state.deep && !state.warningAccepted)) showVerdict(contents, state);
    }

    private static boolean isScanning(State state) {
        return state != null && state.verdict != 3 && state.verdict != 5
                && (state.verdict == 0 || state.deepPending);
    }

    private void refreshScanningIndicator(WebContents contents, State state) {
        boolean changed = contents != indicatorContents
                || state == null || state.generation != indicatorGeneration;
        if (changed || !isScanning(state)) {
            uiHost.removeCallbacks(showScanIndicator);
            scanIndicator.setVisibility(View.GONE);
        }
        indicatorContents = contents;
        indicatorGeneration = state == null ? -1 : state.generation;
        if (isScanning(state) && scanIndicator.getVisibility() != View.VISIBLE) {
            uiHost.removeCallbacks(showScanIndicator);
            uiHost.postDelayed(showScanIndicator, 350);
        }
    }

    private void showScanningIndicator() {
        State state = STATES.get(current());
        Activity owner = activity.get();
        if (owner != null && !owner.isFinishing() && current() == indicatorContents
                && state != null && state.generation == indicatorGeneration && isScanning(state)) {
            scanIndicator.setVisibility(View.VISIBLE);
        }
    }

    private int dp(int value) {
        return Math.round(value * uiHost.getResources().getDisplayMetrics().density);
    }

    private void returnToSafety(WebContents contents, State expected) {
        if (contents == null || STATES.get(contents) != expected) return;
        String safe = expected.lastSafe.isEmpty() ? "chrome://newtab/" : expected.lastSafe;
        contents.getNavigationController().loadUrl(new LoadUrlParams(safe));
    }

    private void showVerdict(WebContents contents, State state) {
        Activity owner = activity.get();
        if (owner == null || owner.isFinishing() || verdictDialog != null) return;
        dialogContents = contents; dialogGeneration = state.generation;
        boolean serviceError = state.verdict == 5;
        AlertDialog.Builder builder = new AlertDialog.Builder(owner)
                .setTitle("PhiShark · " + LABELS[state.verdict])
                .setMessage(serviceError ? "PhiShark hesabı veya koruma hizmeti doğrulanamadı. Bu sayfa güvenli olarak onaylanmadı.\n" + state.detail
                        : state.score.isEmpty() ? "Bu gezinme güvenlik kontrolüyle değerlendirildi."
                        : "Risk skoru: " + state.score)
                .setCancelable(serviceError)
                .setNegativeButton(serviceError ? "Kapat" : "Güvenliğe dön", (dialog, which) -> {
                    if (!serviceError) returnToSafety(contents, state);
                });
        // Only warnings can be continued. The native block has no override.
        if (state.verdict == 2) builder.setPositiveButton("Bu gezinme için devam et", (dialog, which) -> {
            if (STATES.get(contents) == state) {
                state.warningAccepted = true;
            }
        });
        if (serviceError) builder.setPositiveButton("PhiShark hesabı", (dialog, which) -> showAccount(false));
        AlertDialog created = builder.create();
        created.setOnDismissListener(dialog -> {
            if (verdictDialog == created) { verdictDialog = null; dialogContents = null; }
        });
        verdictDialog = created; verdictDialog.show();
    }

    public static void showPanel(Activity owner) {
        PhiSharkBridge bridge = WINDOWS.get(owner);
        if (bridge != null) bridge.showPanel();
    }

    private void showPanel() {
        Activity owner = activity.get(); if (owner == null || owner.isFinishing()) return;
        State state = STATES.get(current());
        new AlertDialog.Builder(owner).setTitle("PhiShark Browser")
                .setMessage("Durum: " + statusLabel(state)
                        + (state == null || state.score.isEmpty() ? "" : "\nRisk skoru: " + state.score)
                        + (state == null || state.detail.isEmpty() ? "" : "\n" + state.detail)
                        + "\n\n" + BrowserAccount.status()
                        + "\n\nGizli modda yalnız URL kontrolü yapılır.")
                .setPositiveButton("Hesap ve koruma", (dialog, which) -> showAccount(false))
                .setNeutralButton("Hakkında", (dialog, which) -> showAbout())
                .setNegativeButton("Kapat", null).show();
    }

    private void accountChanged() {
        refresh();
        if (accountDialog != null && BrowserAccount.signedIn() != accountDialogConnected) {
            accountDialog.dismiss(); accountDialog = null; showAccount(false);
        }
    }

    private void showAbout() {
        Activity owner = activity.get(); if (owner == null || owner.isFinishing()) return;
        new AlertDialog.Builder(owner).setTitle("PhiShark Browser")
                .setMessage("PhiShark hesap koruması ve gizli modda URL kontrolü.\n\n"
                        + "Açık kaynak altyapı: Chromium ve Cromite. İlgili lisanslar ve üçüncü taraf bildirimleri korunur.")
                .setPositiveButton("Açık kaynak lisansları", (dialog, which) -> {
                    WebContents contents = current();
                    if (contents != null) contents.getNavigationController().loadUrl(new LoadUrlParams("chrome://credits/"));
                }).setNeutralButton("Geliştirici ayarları", (dialog, which) -> showSettings())
                .setNegativeButton("Kapat", null).show();
    }

    private void showAccount(boolean firstRun) {
        Activity owner = activity.get();
        if (owner == null || owner.isFinishing() || accountDialog != null) return;
        LinearLayout form = new LinearLayout(owner); form.setOrientation(LinearLayout.VERTICAL);
        form.setPadding(dp(24), dp(20), dp(24), dp(12)); form.setBackgroundColor(0xFF092330);
        ImageView logo = new ImageView(owner); logo.setImageResource(R.drawable.phishark_icon);
        logo.setContentDescription("PhiShark"); form.addView(logo, new LinearLayout.LayoutParams(-1, dp(104)));
        TextView title = new TextView(owner); title.setText("PhiShark Browser"); title.setTextSize(25);
        title.setTextColor(Color.WHITE); title.setGravity(Gravity.CENTER); form.addView(title);
        TextView message = new TextView(owner); message.setTextSize(15); message.setTextColor(0xFFD3E9ED);
        message.setPadding(0, dp(16), 0, dp(16));
        boolean connected = BrowserAccount.signedIn();
        accountDialogConnected = connected;
        message.setText(connected ? "Hesabınız bağlı. Oturumunuz bu cihazda güvenli biçimde hatırlanır."
                : "PhiShark hesabınızla giriş yapın. API anahtarı kopyalamadan korumayı hesabınıza bağlayın; sonraki açılışlarda oturumunuz hatırlansın.");
        form.addView(message);
        CheckBox consent = new CheckBox(owner); consent.setTextColor(Color.WHITE);
        consent.setText("Normal modda tam URL'nin query dahil ve temizlenmiş sayfa içeriğinin analiz için PhiShark'a gönderilmesine izin veriyorum. Gizli modda yalnız URL gönderilir.");
        consent.setChecked(hasDeepConsent());
        if (connected) form.addView(consent);
        AlertDialog.Builder builder = new AlertDialog.Builder(owner).setView(form).setCancelable(!firstRun)
                .setPositiveButton(connected ? "Tarayıcıya devam et" : "PhiShark'a giriş yap", null)
                .setNegativeButton(firstRun ? "Şimdilik atla" : "Kapat", (dialog, which) -> {
                    prefs().edit().putBoolean("phishark.onboarded.v1", true).apply();
                });
        if (connected) builder.setNeutralButton("Hesaptan çık", (dialog, which) -> BrowserAccount.logout());
        AlertDialog created = builder.create(); accountDialog = created;
        created.setOnDismissListener(dialog -> { if (accountDialog == created) accountDialog = null; });
        created.setOnShowListener(dialog -> created.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view -> {
            if (connected) {
                prefs().edit().putBoolean(CONSENT, consent.isChecked()).putBoolean("phishark.onboarded.v1", true)
                        .putLong(VERSION, getSettingsVersion() + 1).apply(); created.dismiss();
            } else {
                created.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
                message.setText("PhiShark giriş sayfası açılıyor…");
                BrowserAccount.start(owner, result -> {
                    if (!created.isShowing()) return;
                    message.setText(result); created.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(true);
                });
            }
        }));
        created.show();
    }

    private void showSettings() {
        Activity owner = activity.get(); if (owner == null || owner.isFinishing()) return;
        LinearLayout form = new LinearLayout(owner); form.setOrientation(LinearLayout.VERTICAL);
        form.setPadding(24, 16, 24, 16);
        EditText base = new EditText(owner); base.setHint("PhiShark API adresi (HTTPS)");
        base.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        base.setText(getApiBase()); form.addView(base);
        EditText key = new EditText(owner); key.setHint("Kişisel API anahtarı (değiştirmek için gir)");
        key.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        form.addView(key);
        CheckBox consent = new CheckBox(owner);
        consent.setText("Normal modda tam URL'nin query dahil ve temizlenmiş sayfa içeriğinin PhiShark'a gönderilmesine izin veriyorum. Gizli mod yalnız URL gönderir.");
        consent.setChecked(hasDeepConsent()); form.addView(consent);
        AlertDialog dialog = new AlertDialog.Builder(owner).setTitle("PhiShark API kurulumu")
                .setView(form).setPositiveButton("Kaydet", null).setNegativeButton("İptal", null)
                .setNeutralButton("Anahtarı sil", (d, which) -> {
                    try { vault().clear(); prefs().edit().remove(CONSENT)
                            .putLong(VERSION, getSettingsVersion() + 1).apply(); }
                    catch (Exception ignored) { new AlertDialog.Builder(owner).setMessage("Anahtar silinemedi.").setPositiveButton("Kapat", null).show(); }
                }).create();
        dialog.setOnShowListener(d -> dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view -> {
            byte[] bytes = null;
            try {
                URI uri = new URI(base.getText().toString().trim());
                if (uri.getHost() == null || uri.getUserInfo() != null || uri.getQuery() != null
                        || uri.getFragment() != null || (!"https".equals(uri.getScheme())
                        && !("http".equals(uri.getScheme()) && "127.0.0.1".equals(uri.getHost())))) {
                    base.setError("Geçerli bir HTTPS API adresi girin"); return;
                }
                if (key.length() > 0) {
                    bytes = key.getText().toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
                    vault().save(bytes);
                    BrowserAccount.useDeveloperKey();
                }
                prefs().edit().putString(BASE, uri.toString()).putBoolean(CONSENT, consent.isChecked())
                        .putLong(VERSION, getSettingsVersion() + 1).apply();
                key.setText(""); dialog.dismiss();
            } catch (Exception ignored) { key.setError("Ayarlar kaydedilemedi"); }
            finally { if (bytes != null) Arrays.fill(bytes, (byte) 0); }
        }));
        dialog.show();
    }
}
