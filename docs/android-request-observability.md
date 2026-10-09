# Per-navigation request diagnostics

Impact query: `TabProtection Preflight Capture ReuseNavigationScan deepPending deadline`.
Verify against the current overlay, not graph snapshots. Provider: browser-process
HTTP submissions and cache hits. Direct consumer: JNI protection panel and the
active-tab scanning indicator. Indirect consumers: existing backend/orchestrator
browser routes. No public payload, storage, callback, quota or server behavior
changes. iOS needs its own implementation; its current behavior is unverified.

Implementation order: native memory-only counters, JNI panel and phase labels,
documented tests, actual Chromium build and emulator checks. Deployment order:
Android APK update only. No production deployment is required.

Count actual analysis POST submissions separately for preflight and deep. Count
cache hits separately, and identify authentication/capacity retry attempts. Reset
on a new navigation generation and discard with the tab; retain no URL, evidence,
credential, scan response or persistent diagnostic log. Repeated same-document
callbacks retain the counters of their existing generation. Redirects, changed
query/path and new documents start a new generation; these counters do not claim
to measure a whole redirect chain or all tabs. Account refresh and page resource
requests are outside the analysis POST counters.

The discreet indicator distinguishes URL checking from content analysis. The
panel identifies the result's phase, so a preflight result cannot be mistaken
for a deep result. Request counts do not alter safety decisions or deadlines.

Validation: pending.
