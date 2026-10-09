// SPDX-License-Identifier: GPL-3.0-only
package org.chromium.chrome.browser.phishark;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.view.WindowManager;

/** Only receives a short-lived PKCE code. Token exchange remains native. */
public final class BrowserAccountActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        android.widget.TextView message = new android.widget.TextView(this);
        message.setText("PhiShark Browser\nHesabınız bağlanıyor…");
        message.setTextColor(android.graphics.Color.WHITE); message.setTextSize(22);
        message.setGravity(android.view.Gravity.CENTER); message.setBackgroundColor(0xFF092330);
        setContentView(message);
        BrowserAccount.callback(getIntent().getDataString(), error -> {
            if (isFinishing()) return;
            if (error == null) {
                android.content.Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
                if (launch != null) {
                    launch.addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK | android.content.Intent.FLAG_ACTIVITY_SINGLE_TOP);
                    startActivity(launch);
                }
                finish();
            }
            else new AlertDialog.Builder(this).setTitle("PhiShark Browser").setMessage(error)
                    .setPositiveButton("Kapat", (dialog, which) -> finish()).setOnCancelListener(dialog -> finish()).show();
        });
    }
}
