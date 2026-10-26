// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import android.content.SharedPreferences;
import org.chromium.chrome.browser.omaha.OmahaPrefUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.LinkedList;
import java.util.List;

/**
 * The First Run Experience fragment that allows the user to accept Terms of Service ("ToS") and
 * Privacy Notice, and to opt-in to the usage statistics and crash reports collection ("UMA",
 * User Metrics Analysis) as defined in the Chrome Privacy Notice.
 */
public class ToSAndUMAFirstRunFragment
        extends Fragment implements FirstRunFragment {
    /** Alerts about some methods once ToSAndUMAFirstRunFragment executes them. */
    public interface Observer {
        /** See {@link #onNativeInitialized}. */
        public void onNativeInitialized();
        public void onPolicyServiceInitialized();
        public void onHideLoadingUIComplete();
    }

    private static boolean sShowUmaCheckBoxForTesting;

    @Nullable
    private static ToSAndUMAFirstRunFragment.Observer sObserver;

    private boolean mNativeInitialized;
    private boolean mPolicyServiceInitialized;
    private boolean mTosButtonClicked;
    private boolean mAllowMetricsAndCrashUploading;
    private boolean mUserInteractedWithUmaCheckbox;

    private Button mAcceptButton;
    private CheckBox mAutoUpdaterCheckBox;
    private boolean mAutoUpdaterChecked;
    private TextView mTosAndPrivacy;
    private View mTitle;
    private View mProgressSpinner;

    private long mTosAcceptedTime;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fre_tosanduma, container, false);
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);
        getPageDelegate().getPolicyLoadListener().onAvailable(this::onPolicyServiceInitialized);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mTitle = view.findViewById(R.id.title);
        mProgressSpinner = view.findViewById(R.id.progress_spinner);
        mProgressSpinner.setVisibility(View.GONE);
        mAcceptButton = (Button) view.findViewById(R.id.terms_accept);
        mAutoUpdaterCheckBox = (CheckBox) view.findViewById(R.id.auto_updater_checkbox);
        mTosAndPrivacy = (TextView) view.findViewById(R.id.tos_and_privacy);

        // Register event listeners.
        mAcceptButton.setOnClickListener((v) -> onTosButtonClicked());
        mAutoUpdaterCheckBox.setOnCheckedChangeListener(
                ((compoundButton, isChecked) -> {
            mAutoUpdaterChecked = isChecked;
        }));

        // Make TextView links clickable.
        mTosAndPrivacy.setMovementMethod(LinkMovementMethod.getInstance());

        updateView();

        // If this page should be skipped, it can be one of the following cases:
        //   1. Native hasn't been initialized yet and this page will be skipped once that happens.
        //   2. The user has moved back to this page after advancing past it. In this case, this
        //      may not even be the same object as before, as the fragment may have been re-created.
        //
        // In case 1, hide all the elements except for Chrome logo and the spinner until native gets
        // initialized at which point the activity will skip the page.
        // We distinguish case 1 from case 2 by the value of |mNativeInitialized|, as that is set
        // via onAttachFragment() from FirstRunActivity - which is before this onViewCreated().
        boolean isW = isWaitingForNativeAndPolicyInit();
        boolean ssw = FirstRunStatus.shouldSkipWelcomePage();
        if (isW && ssw) {
            setSpinnerVisible(true);
        }
    }

    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (mTitle == null) return;
        mTitle.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Override
    public void setUserVisibleHint(boolean isVisibleToUser) {
        super.setUserVisibleHint(isVisibleToUser);

        // This may be called before onViewCreated(), in which case the below is not yet relevant.
        if (mTitle == null) return;

        if (!isVisibleToUser) {
            // Restore original enabled & visibility states, in case the user returns to the page.
            setSpinnerVisible(false);
        } else {
            // On certain versions of Android, the checkbox will appear unchecked upon revisiting
            // the page.  Force it to the end state of the drawable animation as a work around.
            // crbug.com/666258
            mAutoUpdaterCheckBox.jumpDrawablesToCurrentState();
        }
    }

    @Override
    public void onNativeInitialized() {
        assert !mNativeInitialized;

        mNativeInitialized = true;
        tryMarkTermsAccepted(false);

        if (mPolicyServiceInitialized) {
            onNativeAndPolicyServiceInitialized();
        }

        if (sObserver != null) {
            sObserver.onNativeInitialized();
        }
    }

    @Override
    public void reset() {
        // We cannot pass the welcome page when native or policy is not initialized. When this page
        // is revisited, this means this page is persist and we should re-show the ToS And UMA.
        assert !isWaitingForNativeAndPolicyInit();

        setSpinnerVisible(false);
        mAutoUpdaterCheckBox.setChecked(false);
    }

    private void updateView() {
        // Avoid early calls.
        if (getPageDelegate() == null) {
            return;
        }

        updateTosText();

        updateReportCheckbox();
    }

    private SpanInfo buildPrivacyPolicyLink(String suffix, int url) {
        ChromeClickableSpan clickableSpan =
                new ChromeClickableSpan(getContext(), (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(url);
                });

        return new SpanInfo("<PRIVACY_LINK" + suffix + ">", "</PRIVACY_LINK" + suffix + ">", clickableSpan);
    }

    private void updateTosText() {
        List<SpanInfo> spans = new LinkedList<SpanInfo>();

        spans.add(buildPrivacyPolicyLink("1", R.string.privacy_link1));

        spans.add(buildPrivacyPolicyLink("2", R.string.privacy_link2));

        spans.add(buildPrivacyPolicyLink("3", R.string.privacy_link3));

        spans.add(buildPrivacyPolicyLink("4", R.string.privacy_link4));

        String tosString = getString(R.string.bromite_fre_footer_privacy_policy);

        mTosAndPrivacy.setText(SpanApplier.applySpans(tosString, spans.toArray(new SpanInfo[0])));
    }

    private void updateReportCheckbox() {
        mAutoUpdaterCheckBox.setChecked(false);
    }

    private void onPolicyServiceInitialized(boolean onDevicePolicyFound) {
        assert !mPolicyServiceInitialized;

        mPolicyServiceInitialized = true;
        tryMarkTermsAccepted(false);

        if (mNativeInitialized) {
            onNativeAndPolicyServiceInitialized();
        }

        if (sObserver != null) {
            sObserver.onPolicyServiceInitialized();
        }
    }

    private void onNativeAndPolicyServiceInitialized() {
        // Once we have native & policies, Check whether metrics reporting are permitted by policy
        // and update interface accordingly.
        updateView();
    }

    private void onTosButtonClicked() {
        mTosButtonClicked = true;
        mTosAcceptedTime = SystemClock.elapsedRealtime();

        // save updater configuration only on button click
        SharedPreferences.Editor sharedPreferenceEditor = OmahaPrefUtils.getSharedPreferences().edit();
        sharedPreferenceEditor.putBoolean(OmahaPrefUtils.PREF_ALLOW_INLINE_UPDATE, mAutoUpdaterChecked);
        sharedPreferenceEditor.apply();

        tryMarkTermsAccepted(true);
    }

    /**
     * This should be called Tos button is clicked for a fresh new FRE, or when native and policies
     * are initialized if Tos has ever been accepted.
     *
     * @param fromButtonClicked Whether called from {@link #onTosButtonClicked()}.
     */
    private void tryMarkTermsAccepted(boolean fromButtonClicked) {
        boolean isW = isWaitingForNativeAndPolicyInit();
        if (!mTosButtonClicked || isW) {
            if (fromButtonClicked) setSpinnerVisible(true);
            return;
        }

        // In cases where the attempt is triggered other than button click, the ToS should have been
        // accepted by the user already.
        if (!fromButtonClicked) {
            RecordHistogram.recordTimesHistogram("MobileFre.TosFragment.SpinnerVisibleDuration",
                    SystemClock.elapsedRealtime() - mTosAcceptedTime);
        }
        getPageDelegate().acceptTermsOfService(false);
        getPageDelegate().advanceToNextPage();
    }

    private void setSpinnerVisible(boolean spinnerVisible) {
        // When the progress spinner is visible, we hide the other UI elements so that
        // the user can't interact with them.
        boolean otherElementVisible = !spinnerVisible;

        setTosAndUmaVisible(otherElementVisible);
        mTitle.setVisibility(otherElementVisible ? View.VISIBLE : View.INVISIBLE);
        mProgressSpinner.setVisibility(spinnerVisible ? View.VISIBLE : View.GONE);
    }

    private boolean isWaitingForNativeAndPolicyInit() {
        return !mNativeInitialized || !mPolicyServiceInitialized;
    }

    private boolean getUmaCheckBoxInitialState() {
        return false;
    }

    // Exposed methods for ToSAndUMACCTFirstRunFragment

    protected void setTosAndUmaVisible(boolean isVisible) {
        int visibility = isVisible ? View.VISIBLE : View.GONE;

        mAcceptButton.setVisibility(visibility);
        mTosAndPrivacy.setVisibility(visibility);
        mAutoUpdaterCheckBox.setVisibility(visibility);
    }

    protected View getToSAndPrivacyText() {
        return mTosAndPrivacy;
    }

    protected void onHideLoadingUIComplete() {
        if (sObserver != null) {
            sObserver.onHideLoadingUIComplete();
        }
    }

    @VisibleForTesting
    public static void setShowUmaCheckBoxForTesting(boolean showForTesting) {
        sShowUmaCheckBoxForTesting = showForTesting;
    }

    @VisibleForTesting
    public static void setObserverForTesting(ToSAndUMAFirstRunFragment.Observer observer) {
        assert observer == null || sObserver == null;
        sObserver = observer;
    }
}
