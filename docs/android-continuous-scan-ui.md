# Continuous scan indicator and English UI

Impact query: `PhiSharkBridge refreshScanningIndicator updateState setDeepPending BrowserAccount`.
Provider: browser-process preflight/content lifecycle. Direct consumer: JNI UI;
indirect consumers: existing browser APIs (no new requests or schema changes).
Only Android app code/resources and the Mac handoff change. No account, consent,
storage, callback, billing, retry or risk-policy migration is involved.

Implementation order: explicit awaiting-content state, stable active-tab indicator,
English Android resources and native diagnostic messages, tests/build/device
checks. Deployment order: APK update only; no server deployment. iOS needs the
equivalent implementation on Mac, with separate validation.

Keep one static indicator across preflight, document loading and deep capture.
Do not hide it merely because preflight completes or a redirect changes the
generation. Clear immediately on terminal result, failed/cancelled document
navigation, internal page, selected-tab change or activity teardown. Private and
non-consented sessions stop at preflight. Waiting for document content is not a
new API request; existing request counters remain the source of attempt counts.
Waiting for document content is limited to 30 seconds and then becomes
unverified, without interrupting page loading. A later completed document can
still start its existing content analysis. The 10/20-second API deadlines are
unchanged. The indicator uses one static English label; panel details retain the
phase distinction.

PhiShark-specific UI uses English default Android string resources, with no
Turkish override. This follows the user's permission to use English throughout;
it is not a claim that every upstream/browser locale has been translated.
Native fixed diagnostics also become English. Existing browser language settings,
user history/bookmarks and website content are preserved.

Validation: pending.
