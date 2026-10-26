package org.chromium.chrome.browser.omaha;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;

import org.chromium.chrome.browser.app.ChromeActivity;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.omaha.inline.BromiteInlineUpdateController;

public class CromiteUpdateStatusProvider extends UpdateStatusProvider {
    private static final class LazyHolder {
        private static final UpdateStatusProvider INSTANCE = new CromiteUpdateStatusProvider();
    }

    private CromiteUpdateStatusProvider() {
        super(new org.chromium.chrome.browser.phishark.PhiSharkUpdateController());
    }

    /** @return Returns a singleton of {@link UpdateStatusProvider}. */
    public static UpdateStatusProvider getInstance() {
        return LazyHolder.INSTANCE;
    }

    // ApplicationStateListener implementation.
    @Override
    public void onActivityStateChange(Activity changedActivity, @ActivityState int newState) {
        boolean hasActiveActivity = false;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity == null || !(activity instanceof ChromeActivity)) continue;

            hasActiveActivity |=
                    ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED;
            if (hasActiveActivity) break;
        }

        mInlineController.setEnabled(hasActiveActivity);
    }
}
