// SPDX-License-Identifier: GPL-3.0-only
package org.chromium.chrome.browser.phishark;

import android.app.Activity;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.omaha.inline.InlineUpdateController;

/** Prevents this fork from polling/downloading upstream Cromite packages. */
public final class PhiSharkUpdateController implements InlineUpdateController {
    @Override public void setCallback(Runnable callback) { }
    @Override public void setEnabled(boolean enabled) { }
    @Override public Integer getStatus() { return UpdateStatusProvider.UpdateState.NONE; }
    @Override public String getUpdateUrl() { return ""; }
    @Override public String getDownloadUrl() { return ""; }
    @Override public String getVulnerableVersionDocUrl() { return ""; }
    @Override public void startUpdate(Activity activity) { }
    @Override public void completeUpdate() { }
}
