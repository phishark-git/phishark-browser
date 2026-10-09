# Android request deduplication and quiet browsing

## Scope and implementation plan

The initial workspace query was `PhiShark Preflight Capture DidFinishNavigation
request deduplication`. Verify against the current Android overlay: throttle
callbacks and the commit observer both feed `TabProtection::Preflight`; load,
commit and preflight completion feed `Capture`. History API events currently
reset the generation even when the canonical URL and document are unchanged.
That invalidates the once-per-generation capture guard. Dynamic HTML also changes
the deep cache key, so that cache cannot substitute for navigation deduplication.

Provider: Android browser-process navigation state. Direct consumer: JNI status
and native dialogs/menu. Indirect consumers: the existing ephemeral backend and
orchestrator (fewer redundant submissions); no schema, callbacks, billing or
storage contract changes. iOS must implement the same behavior independently.

Implementation order: guard duplicate navigation events and pending preflight;
remove the floating status UI and add a native menu entry; regression fixtures
and native tests; x64 build and emulator request-count checks. Deployment order:
Android app update only, no server deployment. Preserve explicit redirects,
changed query/path, reload/new documents, private isolation, cancellation and
existing warning/block thresholds. Capacity/auth retries remain bounded and
must be reported separately from duplicate navigation events.

Normal browsing shows no floating result badge. Explicit menu access retains
account/protection diagnostics. Only risk decisions automatically interrupt
browsing; incomplete and service-error results are never converted into safe.

## Validation

Measured on the x64 API 35 emulator, using only synthetic loopback fixtures:

| Case | Preflight POSTs | Deep POSTs | Observation |
| --- | ---: | ---: | --- |
| Previous APK `690de3c1`, one load + five same-URL history events | 1 | 6 | Duplicate reproduced; changing HTML defeated the body cache |
| APK `bd7885ee`, identical event sequence | 1 | 1 | All five events completed; no badge |
| Same events with a three-second deep response | 1 | 1 | Running deep request retained |
| Changed query on the same document | +1 | +1 | Meaningful URL change remains checked |
| Same-document change to blocked URL | 1 | 0 | Score 86; document replaced with blank and native block; target page GET count 0 |
| Two redirect hops + final page | 3 | 1 | Both intermediate targets checked; two redirect GETs |
| Deep block | 1 | 1 | Score 61; blank replacement and native block, no bypass |

All fixture evidence checks reported zero privacy rejections. These counters
measure the browser's API POSTs, not downstream analyzer fan-out. No production
logs, account credentials or evidence bodies were inspected. Cross-tab request
coalescing remains incomplete. Capacity/auth retry logic is unchanged: those
explicit retries and real redirects can legitimately exceed two POSTs.

Tests: 8 shared JS tests, 42 C++ decision vectors plus navigation invariants,
19 OAuth assertions and 5 branding tests passed. The native regression covers
duplicate callbacks, same-document reuse, uncommitted documents, reloads,
redirects and changed query. Actual Chromium Java/JNI/C++ builds passed. Final
source `bd7885ee` built the x64 APK/AAB in 4m20.77s after the initial UI build.
The existing upstream Android XR `UsedByNative` warning remained nonfatal.

APK SHA-256: `6d1d4aeb3b25b07500aab487d4aa0d843f9e6abad47f85f20d5fcee6e90b86af`.
AAB SHA-256: `369d5772333185f82eba8d912d09d987702e770e8916a1fc9671fddb6d162855`.
`adb install -r` succeeded without clearing app data. The original command-line
file was restored, absence of `--phishark-local-fixtures` verified, and the app
restarted after testing. This is a development-signed x64 build; ARM64 physical
devices, iOS and store acceptance are not established by these tests.
The normal-mode menu entry opened the protection panel, which displayed the
retained connected account. No login or consent entry was repeated. The panel
was closed and the browser left on the ordinary new-tab page without a badge.

Browser/component/workspace graphs were refreshed and the initial query repeated.
Browser: 534 nodes / 1,082 raw edges; component: 928 edges; workspace: 13,088 nodes
/ 28,785 edges with no dangling aggregate endpoints. The final query returned 148 nodes
with budget truncation. Component generation excluded 143 external AST edges and
reported four conceptual nodes without source files. Mixed branch snapshots,
older node IDs and incomplete document semantics still limit impact analysis.
There is no server deployment or backend API change. The Mac handoff describes
the equivalent iOS work, which must be validated separately on Mac.
