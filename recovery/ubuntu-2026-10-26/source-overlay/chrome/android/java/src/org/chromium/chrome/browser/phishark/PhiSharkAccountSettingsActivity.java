// SPDX-License-Identifier: GPL-3.0-only
package org.chromium.chrome.browser.phishark;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.WindowManager;
import android.view.Window;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;

/** Native account screen shared by the app menu and browser Settings. */
public final class PhiSharkAccountSettingsActivity extends Activity {
    private static final int BACKGROUND = 0xFF092330;
    private static final String CONSENT = "phishark.deep_consent.v1";
    private LinearLayout content;
    private AlertDialog privacyDialog;
    private final Runnable accountChanged = () -> { if (!isFinishing()) render(); };

    private SharedPreferences prefs() { return ContextUtils.getAppSharedPreferences(); }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        setTitle(R.string.phishark_account_title);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true); scroll.setBackgroundColor(BACKGROUND);
        content = new LinearLayout(this); content.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(content, new ScrollView.LayoutParams(-1, -2));
        scroll.setOnApplyWindowInsetsListener((view, insets) -> {
            // Pinned Chromium supports Android 10 (API 29) as well as API 35.
            content.setPadding(dp(24), insets.getSystemWindowInsetTop() + dp(16),
                    dp(24), insets.getSystemWindowInsetBottom() + dp(24));
            return insets;
        });
        setContentView(scroll);
        BrowserAccount.addListener(accountChanged);
        render();
    }
    @Override public void onResume() {
        super.onResume(); BrowserAccount.loadProfile(); render();
    }
    @Override public void onDestroy() {
        BrowserAccount.removeListener(accountChanged);
        if (privacyDialog != null) privacyDialog.dismiss();
        super.onDestroy();
    }
    private TextView label(LinearLayout parent, String value, int size, int color) {
        TextView text = new TextView(this); text.setText(value); text.setTextSize(size);
        text.setTextColor(color); text.setPadding(0, dp(6), 0, dp(6));
        parent.addView(text, new LinearLayout.LayoutParams(-1, -2)); return text;
    }
    private Button action(LinearLayout parent, int title, int color, Runnable click) {
        Button button = new Button(this); button.setText(title); button.setAllCaps(false);
        button.setTextColor(color); button.setTextSize(16); button.setGravity(Gravity.CENTER);
        GradientDrawable shape = new GradientDrawable(); shape.setColor(0xFF123745);
        shape.setCornerRadius(dp(14)); shape.setStroke(dp(1), 0xFF306170);
        button.setBackground(shape); button.setPadding(dp(16), dp(14), dp(16), dp(14));
        LinearLayout.LayoutParams layout = new LinearLayout.LayoutParams(-1, -2);
        layout.topMargin = dp(12); parent.addView(button, layout);
        button.setOnClickListener(view -> click.run()); return button;
    }
    private void render() {
        content.removeAllViews();
        LinearLayout header = new LinearLayout(this); header.setGravity(Gravity.CENTER_VERTICAL);
        Button back = new Button(this); back.setText("‹"); back.setTextSize(30);
        back.setTextColor(Color.WHITE); back.setBackgroundColor(Color.TRANSPARENT);
        back.setContentDescription(getString(R.string.phishark_account_back));
        back.setOnClickListener(view -> finish()); header.addView(back, new LinearLayout.LayoutParams(dp(48), dp(48)));
        ImageView logo = new ImageView(this); logo.setImageResource(R.drawable.phishark_icon);
        logo.setContentDescription("PhiShark"); header.addView(logo, new LinearLayout.LayoutParams(dp(36), dp(36)));
        content.addView(header);
        label(content, getString(R.string.phishark_account_title), 28, Color.WHITE);

        boolean connected = BrowserAccount.signedIn();
        LinearLayout profile = new LinearLayout(this); profile.setOrientation(LinearLayout.VERTICAL);
        profile.setPadding(dp(20), dp(20), dp(20), dp(20));
        GradientDrawable card = new GradientDrawable(); card.setColor(0xFF123745); card.setCornerRadius(dp(20));
        profile.setBackground(card);
        LinearLayout.LayoutParams cardLayout = new LinearLayout.LayoutParams(-1, -2); cardLayout.topMargin = dp(20);
        content.addView(profile, cardLayout);
        String name = BrowserAccount.displayName(), email = BrowserAccount.email();
        label(profile, connected && !name.isEmpty() ? name : getString(R.string.phishark_account_identity), 22, Color.WHITE);
        if (connected && !email.isEmpty()) label(profile, email, 15, 0xFFD3E9ED);
        label(profile, getString(connected ? R.string.phishark_account_connected : R.string.phishark_account_disconnected), 14, 0xFF6DE1D2);
        if (connected && name.isEmpty() && email.isEmpty()) {
            label(profile, getString(BrowserAccount.isRefreshing() ? R.string.phishark_profile_loading : R.string.phishark_profile_unavailable), 14, 0xFFD3E9ED);
        }

        if (connected) {
            label(content, getString(prefs().getBoolean(CONSENT, false)
                    ? R.string.phishark_page_analysis_on : R.string.phishark_page_analysis_off), 15, 0xFFD3E9ED);
            action(content, R.string.phishark_account_protection_settings, Color.WHITE, this::showProtectionSettings);
            action(content, R.string.phishark_ui_044, 0xFFFFB5B5, () -> {
                BrowserAccount.logout(); render();
            });
        } else {
            TextView message = label(content, getString(R.string.phishark_account_sign_in_description), 15, 0xFFD3E9ED);
            Button signIn = action(content, R.string.phishark_ui_042, Color.WHITE, () -> {});
            signIn.setOnClickListener(view -> {
                signIn.setEnabled(false); message.setText(R.string.phishark_ui_045);
                BrowserAccount.start(this, result -> {
                    if (isFinishing() || isDestroyed()) return;
                    message.setText(result); signIn.setEnabled(true);
                });
            });
        }
    }
    private void showProtectionSettings() {
        CheckBox consent = new CheckBox(this); consent.setPadding(dp(24), dp(16), dp(24), dp(16));
        consent.setText(R.string.phishark_ui_040); consent.setChecked(prefs().getBoolean(CONSENT, false));
        privacyDialog = new AlertDialog.Builder(this).setTitle(R.string.phishark_account_protection_settings)
                .setView(consent).setNegativeButton(R.string.phishark_ui_051, null)
                .setPositiveButton(R.string.phishark_ui_050, (dialog, which) -> {
                    // Signing out while this dialog is open cannot restore consent.
                    if (!BrowserAccount.signedIn()) return;
                    if (consent.isChecked() != prefs().getBoolean(CONSENT, false)) {
                        prefs().edit().putBoolean(CONSENT, consent.isChecked())
                                .putLong("phishark.settings_version", prefs().getLong("phishark.settings_version", 0) + 1).apply();
                    }
                    render();
                }).create();
        privacyDialog.setOnDismissListener(dialog -> privacyDialog = null);
        privacyDialog.show();
    }
}
