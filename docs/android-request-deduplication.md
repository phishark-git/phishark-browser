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

Pending; record observed counts and build evidence after execution. Graph data
includes mixed branch snapshots and incomplete document semantics; source code
is authoritative, and cross-tab coalescing remains outside this correction.
