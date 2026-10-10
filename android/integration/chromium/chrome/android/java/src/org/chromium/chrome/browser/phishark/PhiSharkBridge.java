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
    private static String text(int id) { return ContextUtils.getApplicationContext().getString(id); }

    private static final String BASE = "phishark.api_base";
    private static final String CONSENT = "phishark.deep_consent.v1";
    private static final String VERSION = "phishark.settings_version";
    private static final int[] LABELS = {R.string.phishark_ui_001, R.string.phishark_ui_002, R.string.phishark_ui_003,
            R.string.phishark_ui_004, R.string.phishark_ui_005, R.string.phishark_ui_006};
    private static final WeakHashMap<WebContents, State> STATES = new WeakHashMap<>();
    private static final WeakHashMap<Activity, PhiSharkBridge> WINDOWS = new WeakHashMap<>();
    private final WeakReference<Activity> activity;
    private final ActivityTabProvider tabs;
    private final View uiHost;
    private final TextView scanIndicator;
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
        boolean awaitingContent;
        String requestCounts = text(R.string.phishark_ui_007);
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
        scanIndicator.setText(text(R.string.phishark_ui_008));
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
            long generation, String score, String lastSafe, boolean deep, String detail, int urlVerdict,
            boolean awaitingContent) {
        if (verdict < 0 || verdict >= LABELS.length || contents == null) return;
        State previous = STATES.get(contents);
        if (previous != null && generation < previous.generation) return;
        State updated = new State(verdict, generation, score, lastSafe, deep, detail, urlVerdict);
        updated.awaitingContent = awaitingContent;
        if (previous != null && previous.generation == generation) updated.requestCounts = previous.requestCounts;
        STATES.put(contents, updated);
        for (PhiSharkBridge window : WINDOWS.values()) window.refresh();
    }

    @CalledByNative private static void updateRequestCounts(
            @JniType("content::WebContents*") WebContents contents, long generation,
            int preflight, int deep, int preflightCache, int deepCache, int authRetry, int capacityRetry,
            int htmlBytes, int pngBytes, int httpStatus, int networkCode) {
        State state = STATES.get(contents);
        if (state == null || state.generation != generation) return;
        state.requestCounts = text(R.string.phishark_ui_009) + preflight + text(R.string.phishark_ui_010) + deep
                + text(R.string.phishark_ui_011) + preflightCache + text(R.string.phishark_ui_010) + deepCache
                + text(R.string.phishark_ui_012) + authRetry + text(R.string.phishark_ui_013) + capacityRetry;
        if (deep > 0) {
            state.requestCounts += "\nDeep upload: HTML " + htmlBytes + " B · PNG " + pngBytes + " B"
                    + "\nDeep response: " + (httpStatus == 0 ? "no HTTP response" : "HTTP " + httpStatus)
                    + " · network " + networkCode;
        }
    }

    private WebContents current() {
        Tab tab = tabs.get(); return tab == null ? null : tab.getWebContents();
    }

    @CalledByNative private static void setDeepPending(
            @JniType("content::WebContents*") WebContents contents, long generation) {
        State state = STATES.get(contents);
        if (state == null || state.generation != generation || state.verdict == 3) return;
        state.deepPending = true;
        state.awaitingContent = false;
        for (PhiSharkBridge window : WINDOWS.values()) window.refresh();
    }

    private static String statusLabel(State state) {
        if (state != null && state.awaitingContent) return text(R.string.phishark_waiting_for_content);
        if (state == null) return text(R.string.phishark_ui_014);
        if (state.deepPending) return (state.verdict == 2 ? text(R.string.phishark_ui_015) : "") + text(R.string.phishark_ui_016);
        if (state.verdict == 1 && !state.deep) return text(R.string.phishark_ui_017);
        // A URL verdict is scoped context. It never overrides a deep block,
        // warning or service error, nor turns incomplete capture into safe.
        if (state.verdict == 4 && state.urlVerdict == 1) return text(R.string.phishark_ui_018);
        if (state.verdict == 4 && state.urlVerdict == 2) return text(R.string.phishark_ui_019);
        return text(LABELS[state.verdict]);
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
                && state.deepPending;
    }

    private void refreshScanningIndicator(WebContents contents, State state) {
        Activity owner = activity.get();
        scanIndicator.setVisibility(owner != null && !owner.isFinishing()
                && contents == current() && isScanning(state) ? View.VISIBLE : View.GONE);
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
                .setTitle("PhiShark · " + text(LABELS[state.verdict]))
                .setMessage(serviceError ? text(R.string.phishark_ui_020) + state.detail
                        : state.score.isEmpty() ? text(R.string.phishark_ui_021)
                        : text(R.string.phishark_ui_022) + state.score)
                .setCancelable(serviceError)
                .setNegativeButton(serviceError ? text(R.string.phishark_ui_023) : text(R.string.phishark_ui_024), (dialog, which) -> {
                    if (!serviceError) returnToSafety(contents, state);
                });
        // Only warnings can be continued. The native block has no override.
        if (state.verdict == 2) builder.setPositiveButton(text(R.string.phishark_ui_025), (dialog, which) -> {
            if (STATES.get(contents) == state) {
                state.warningAccepted = true;
            }
        });
        // Keep the current result's phase/counts inspectable even when the
        // blocking dialog covers the app menu. This does not resume navigation.
        if (!serviceError) builder.setNeutralButton(text(R.string.phishark_ui_032),
                (dialog, which) -> showPanel());
        if (serviceError) builder.setPositiveButton(text(R.string.phishark_ui_026), (dialog, which) -> showAccount(false));
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
                .setMessage(text(R.string.phishark_ui_027) + statusLabel(state)
                        + (state == null ? "" : text(R.string.phishark_ui_028)
                                + (state.deepPending || state.deep ? text(R.string.phishark_ui_029) : "URL (preflight)"))
                        + (state == null || state.score.isEmpty() ? "" : text(R.string.phishark_ui_030) + state.score)
                        + (state == null || state.detail.isEmpty() ? "" : "\n" + state.detail)
                        + (state == null ? "" : "\n\n" + state.requestCounts)
                        + "\n\n" + BrowserAccount.status()
                        + text(R.string.phishark_ui_031))
                .setPositiveButton(text(R.string.phishark_ui_032), (dialog, which) -> showAccount(false))
                .setNeutralButton(text(R.string.phishark_ui_033), (dialog, which) -> showAbout())
                .setNegativeButton(text(R.string.phishark_ui_023), null).show();
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
                .setMessage(text(R.string.phishark_ui_034)
                        + text(R.string.phishark_ui_035))
                .setPositiveButton(text(R.string.phishark_ui_036), (dialog, which) -> {
                    WebContents contents = current();
                    if (contents != null) contents.getNavigationController().loadUrl(new LoadUrlParams("chrome://credits/"));
                }).setNeutralButton(text(R.string.phishark_ui_037), (dialog, which) -> showSettings())
                .setNegativeButton(text(R.string.phishark_ui_023), null).show();
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
        message.setText(connected ? text(R.string.phishark_ui_038)
                : text(R.string.phishark_ui_039));
        form.addView(message);
        CheckBox consent = new CheckBox(owner); consent.setTextColor(Color.WHITE);
        consent.setText(text(R.string.phishark_ui_040));
        consent.setChecked(hasDeepConsent());
        if (connected) form.addView(consent);
        AlertDialog.Builder builder = new AlertDialog.Builder(owner).setView(form).setCancelable(!firstRun)
                .setPositiveButton(connected ? text(R.string.phishark_ui_041) : text(R.string.phishark_ui_042), null)
                .setNegativeButton(firstRun ? text(R.string.phishark_ui_043) : text(R.string.phishark_ui_023), (dialog, which) -> {
                    prefs().edit().putBoolean("phishark.onboarded.v1", true).apply();
                });
        if (connected) builder.setNeutralButton(text(R.string.phishark_ui_044), (dialog, which) -> BrowserAccount.logout());
        AlertDialog created = builder.create(); accountDialog = created;
        created.setOnDismissListener(dialog -> { if (accountDialog == created) accountDialog = null; });
        created.setOnShowListener(dialog -> created.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view -> {
            if (connected) {
                prefs().edit().putBoolean(CONSENT, consent.isChecked()).putBoolean("phishark.onboarded.v1", true)
                        .putLong(VERSION, getSettingsVersion() + 1).apply(); created.dismiss();
            } else {
                created.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
                message.setText(text(R.string.phishark_ui_045));
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
        EditText base = new EditText(owner); base.setHint(text(R.string.phishark_ui_046));
        base.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        base.setText(getApiBase()); form.addView(base);
        EditText key = new EditText(owner); key.setHint(text(R.string.phishark_ui_047));
        key.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        form.addView(key);
        CheckBox consent = new CheckBox(owner);
        consent.setText(text(R.string.phishark_ui_048));
        consent.setChecked(hasDeepConsent()); form.addView(consent);
        AlertDialog dialog = new AlertDialog.Builder(owner).setTitle(text(R.string.phishark_ui_049))
                .setView(form).setPositiveButton(text(R.string.phishark_ui_050), null).setNegativeButton(text(R.string.phishark_ui_051), null)
                .setNeutralButton(text(R.string.phishark_ui_052), (d, which) -> {
                    try { vault().clear(); prefs().edit().remove(CONSENT)
                            .putLong(VERSION, getSettingsVersion() + 1).apply(); }
                    catch (Exception ignored) { new AlertDialog.Builder(owner).setMessage(text(R.string.phishark_ui_053)).setPositiveButton(text(R.string.phishark_ui_023), null).show(); }
                }).create();
        dialog.setOnShowListener(d -> dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view -> {
            byte[] bytes = null;
            try {
                URI uri = new URI(base.getText().toString().trim());
                if (uri.getHost() == null || uri.getUserInfo() != null || uri.getQuery() != null
                        || uri.getFragment() != null || (!"https".equals(uri.getScheme())
                        && !("http".equals(uri.getScheme()) && "127.0.0.1".equals(uri.getHost())))) {
                    base.setError(text(R.string.phishark_ui_054)); return;
                }
                if (key.length() > 0) {
                    bytes = key.getText().toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
                    vault().save(bytes);
                    BrowserAccount.useDeveloperKey();
                }
                prefs().edit().putString(BASE, uri.toString()).putBoolean(CONSENT, consent.isChecked())
                        .putLong(VERSION, getSettingsVersion() + 1).apply();
                key.setText(""); dialog.dismiss();
            } catch (Exception ignored) { key.setError(text(R.string.phishark_ui_055)); }
            finally { if (bytes != null) Arrays.fill(bytes, (byte) 0); }
        }));
        dialog.show();
    }
}
