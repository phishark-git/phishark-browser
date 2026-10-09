# Security architecture

## Request and decision boundaries

The browser process owns account credentials (primary onboarding) or a personal API key (developer mode). Analysis uses HTTPS only to `/api/v1/browser/preflight` and `/api/v1/browser/deep`. Separate native PKCE pairing uses `/api/browser/auth/*`, scope `browser:scan` and exact callback `io.phishark.browser:/oauth/callback`. Tokens/verifiers are never exposed to renderers. There is no fallback to history-producing endpoints. The public backend validates a scoped active browser Bearer session or `X-API-Key`, preserves numeric quota reservation/refund, and calls the internal `/v2/browser-scans/preflight` or `/v2/browser-scans/deep` with `BROWSER_SCAN_INTERNAL_TOKEN`. This token is server-only. Account/developer modes cannot send both credentials. Backend and dashboard changes require deployment before live account use.

Orchestrator's request-scoped `Ephemeral` field is non-JSON. Public callers cannot opt ordinary scans out of persistence, and browser callers cannot opt into it. Existing profiles and response fields remain intact. Browser scans skip start metadata, final metadata, completed result/artifact persistence, batch persistence and callbacks. The synchronous owner cancels work and removes transient state/fetcher evidence on completion, failure and cancellation. The persistence gate captures the privacy flag atomically before state can be deleted.

Internal missing token returns unavailable; bad token returns unauthorized. Backend converts configuration/authentication mismatches into `BROWSER_NOT_CONFIGURED`, rather than a safe verdict. Mobile deep additionally requires the existing prompt-policy flag; flag-disabled deep returns a service error. Gin logging/recovery excludes browser requests and does not dump query strings, headers or exception text. Normal request logging and normal scan history behavior are preserved.

## Navigation

Each tab has a monotonically increasing navigation generation. Begin cancels old work. Only matching results may change state. A confirmed block is sticky for that navigation and cannot be reopened by a late safe result. Redirects get a new target check; popup/restore/intent/history/same-document paths need native integration tests. The native security screen offers return to the last confirmed safe page, or a new tab if none exists; it is excluded from scanning.

Preflight runs before document navigation where the engine allows. A medium preflight result resumes with a provisional risk indicator and schedules deep analysis. Deep warnings pause interaction and offer back or continuation for this navigation only. Confirmed threat and prompt-policy blocks provide no bypass. Post-load detection stops the page and replaces it, but cannot undo previously executed JavaScript.

## Privacy and deadlines

Normal-mode consent must explain full URL transmission including query and page content analysis. Private mode is URL-only even if normal consent exists. Cookies, authorization headers and form values are excluded. Screenshot input regions must be masked before sending; frame scaling, scrolling, shadow DOM and custom editable controls require device verification. If sanitization or masking cannot be established, omit that evidence and display unverified rather than sending unsafe capture.

Caches are memory-only, per normal/private session: preflight 600 seconds and deep 120 seconds keyed by canonical URL plus evidence hash. Clear private cache on private-session teardown; pending requests cannot repopulate it. Requests are deduplicated and cancelable. End-to-end client deadlines are 10/20 seconds including response consumption and at most one capacity-429 retry. Quota errors are not capacity retries. Temporary errors stay unverified; service setup/auth/quota errors are distinct.

## Current acceptance boundary

The JS code is an executable reference. Android's applied overlay owns native
throttle/commit observers, bounded HTTPS transport, per-tab memory caches,
generation cancellation, JNI vault access and a native status/settings/dialog UI.
Its C++ and Java source compile checks passed against the actual Chromium SDK;
the actual x64 APK passed basic preflight/deep block, return-to-safety and private
URL-only emulator checks. C++ policy is tested
independently. The unmodified ARM64 APK/AAB built but failed emulator launch;
the local native x64 baseline built and loaded the fixture page. The integrated
ARM64 PhiShark APK/AAB built with the pre-account overlay; physical launch remains unverified.
The separate Mac branch `9064f050` reports baseline/integrated simulator builds
and 9 Swift tests passed using Xcode 26.6 experiment mode; native account login,
pinned Xcode 26.5 and physical iPhone acceptance remain unverified. DNS enforcement,
cross-tab request coalescing, complete address-bar UI and capture pixel masking
remain incomplete. Partial HTML deep results cannot label a page safe; screenshots
are omitted. The x64 fixture APK is a runnable prototype; full acceptance is pending.

[Pinned integration points](native-integration-points.md) record the actual
Chromium registry/no-URL-loader interfaces and Firefox action/async-response
callbacks. [Local fixtures](local-device-fixtures.md) provide concrete device
inputs; neither document substitutes for native execution evidence.
