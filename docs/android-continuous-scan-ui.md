# Continuous scan indicator and English UI

Historical behavior: the user's later deep-only preference supersedes this
preflight/document/deep indicator span. See
[current indicator and evidence audit](android-deep-only-indicator.md).

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

## Validation

Source `9c6cbe6d`: 8 shared tests, 42 C++ decision vectors/navigation invariants,
19 OAuth assertions and 5 branding tests passed. Actual Chromium C++/Java/JNI
and Android resources compiled; x64 APK/AAB packaging passed in 6m25.68s.
The existing upstream XR `UsedByNative` warning remained nonfatal.

The synthetic `handoff-slow` fixture delays preflight by 1 second, document GET
by 1.5 seconds and deep by 3 seconds. Emulator UI snapshots at 704 ms, 1,704 ms
and 3,299 ms showed the same visible English indicator; the 6,191 ms snapshot
showed completion with no indicator. These are sampled transitions, not a
frame-by-frame recording. The server counted exactly 1 preflight and 1 deep.
The protection menu/panel displayed English state, phase, score, partial-capture
detail, counters, connected-account status and buttons. The counts matched the
server. A separate deep-block fixture returned score 61: the page was replaced
with blank, “PhiShark · Blocked” and “Back to safety” displayed in English, with
no bypass. Returning to safety cleared the indicator. Total: 2 preflight / 2 deep
POSTs, zero fixture privacy rejections.

In-place installation and launch succeeded without clearing app data. Original
command-line settings were restored, fixture mode verified off, and the local
server stopped. APK SHA-256:
`14d2238149396aaa0de88f614986cdcfe8e5ee7a64ec1563266af9b49823b952`.
AAB SHA-256:
`5deb86be6e89dcd8a9f255d51211e53af141e88fc118b50e2d5bd15b89f597d4`.

All 80 new default string resources compiled. Turkish characters were absent
from the affected PhiShark Java/native UI and integration text. English fallback
is intentional; browser locale switching, every onboarding/error branch,
private-mode continuity and the 30-second document-wait expiry have not received
new device acceptance in this test. iOS work remains pending on Mac. Full mobile
acceptance and production runtime claims remain separate.

Browser code graph, component graph and workspace graph refreshed; the original
query repeated and returned 96 nodes (budget truncated). Browser: 568 nodes /
1,165 raw edges; component: 1,007 edges; workspace: 13,122 nodes / 28,864 edges,
zero dangling aggregate endpoints. The component excludes 147 external AST
references and reports four conceptual nodes missing source files. Markdown/XML
semantics, older node IDs, mixed branch snapshots and outdated planned map fields
limit graph completeness; current source and actual build/device evidence are
authoritative. No unresolved server consumer or server rollout is introduced;
iOS behavior must be implemented and tested independently on Mac.
