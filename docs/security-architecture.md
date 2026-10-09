# Security architecture

## Request and decision boundaries

The browser process owns the personal API key and sends HTTPS requests only to `/api/v1/browser/preflight` and `/api/v1/browser/deep`. There is no fallback to history-producing endpoints. The public backend validates `X-API-Key`, preserves numeric quota reservation/refund, and calls the internal `/v2/browser-scans/preflight` or `/v2/browser-scans/deep` with `BROWSER_SCAN_INTERNAL_TOKEN`. This token is server-only; it is not the user's key.

Orchestrator's request-scoped `Ephemeral` field is non-JSON. Public callers cannot opt ordinary scans out of persistence, and browser callers cannot opt into it. Existing profiles and response fields remain intact. Browser scans skip start metadata, final metadata, completed result/artifact persistence, batch persistence and callbacks. The synchronous owner cancels work and removes transient state/fetcher evidence on completion, failure and cancellation. The persistence gate captures the privacy flag atomically before state can be deleted.

Internal missing token returns unavailable; bad token returns unauthorized. Backend converts configuration/authentication mismatches into `BROWSER_NOT_CONFIGURED`, rather than a safe verdict. Mobile deep additionally requires the existing prompt-policy flag; flag-disabled deep returns a service error. Gin logging/recovery excludes browser requests and does not dump query strings, headers or exception text. Normal request logging and normal scan history behavior are preserved.

## Navigation

Each tab has a monotonically increasing navigation generation. Begin cancels old work. Only matching results may change state. A confirmed block is sticky for that navigation and cannot be reopened by a late safe result. Redirects get a new target check; popup/restore/intent/history/same-document paths need native integration tests. The native security screen offers return to the last confirmed safe page, or a new tab if none exists; it is excluded from scanning.

Preflight runs before document navigation where the engine allows. A medium preflight result resumes with a provisional risk indicator and schedules deep analysis. Deep warnings pause interaction and offer back or continuation for this navigation only. Confirmed threat and prompt-policy blocks provide no bypass. Post-load detection stops the page and replaces it, but cannot undo previously executed JavaScript.

## Privacy and deadlines

Normal-mode consent must explain full URL transmission including query and page content analysis. Private mode is URL-only even if normal consent exists. Cookies, authorization headers and form values are excluded. Screenshot input regions must be masked before sending; frame scaling, scrolling, shadow DOM and custom editable controls require device verification. If sanitization or masking cannot be established, omit that evidence and display unverified rather than sending unsafe capture.

Caches are memory-only, per normal/private session: preflight 600 seconds and deep 120 seconds keyed by canonical URL plus evidence hash. Clear private cache on private-session teardown; pending requests cannot repopulate it. Requests are deduplicated and cancelable. End-to-end client deadlines are 10/20 seconds including response consumption and at most one capacity-429 retry. Quota errors are not capacity retries. Temporary errors stay unverified; service setup/auth/quota errors are distinct.

## Current acceptance boundary

The JS code is an executable reference, not the app's transport. C++ policy is compiled and tested independently. Keystore compiled against the Chromium SDK but awaits device verification; the Swift package awaits Mac compilation. Throttle/delegate wiring, DNS enforcement, native evidence capture, actual UI, in-memory native caches and secure native networking remain gated on successful upstream builds. No APK/AAB/IPA is claimed yet.

[Pinned integration points](native-integration-points.md) record the actual
Chromium registry/no-URL-loader interfaces and Firefox action/async-response
callbacks. [Local fixtures](local-device-fixtures.md) provide concrete device
inputs; neither document substitutes for native execution evidence.
