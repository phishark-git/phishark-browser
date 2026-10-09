# Browser account implementation

Requested 2026-10-09: use the extension-style account pairing experience and
present PhiShark branding from the first launch. This supersedes manual API-key
entry as the primary onboarding flow; developer API-key configuration remains
compatible. Deployment and platform acceptance are separate gates.

## Contract and implementation order

1. Backend provider: dedicated `browser` client, exact
   `io.phishark.browser:/oauth/callback`, S256 PKCE and `/api/browser/auth/*`.
   Browser Bearer credentials may access only the two ephemeral scan routes.
   Existing API-key requests and old client callbacks remain compatible.
   Account usage uses the existing user quota reservation/refund path; no fake
   API key or persistent scan path is introduced.
2. Dashboard consumer: `/browser/connect` preserves login return state and
   validates the exact browser callback before handing the one-time code back.
   Work from the existing mobile-auth branch in a separate browser worktree.
3. Native consumer: first-launch account pairing, native encrypted credentials,
   state/expiry validation, single-flight refresh and explicit sign-out. Normal
   content-analysis consent remains separate; private mode stays URL-only.
4. Branding: use the existing PhiShark mark; replace product-facing upstream
   labels, first-run and new-tab branding while retaining legal notices.

Direct consumers are Android and iOS and the dashboard pairing screen. Indirect
dependencies are user/organization entitlement, quota and orchestrator. Auth
storage is allowed; scan URL/evidence persistence and callbacks remain forbidden
on browser paths. Existing extension, mobile-app, Safari and Outlook sessions
must remain compatible. No extension code change is required.

Deployment order: privacy providers → orchestrator → compatible backend →
dashboard → mobile builds. Backend `deploy-self-hosted.yml` and dashboard
`deploy-hetzner.yml` are the existing release paths; neither is triggered here.
Runtime and enabled production features remain unknown.

Backend test/build, dashboard test/build and browser/native compilation/device
checks must be reported independently. iOS integration requires the Mac baseline.

## Implemented and validation boundary

Backend: dedicated browser PKCE client, account-authenticated ephemeral routes,
scope/session/account/organization validation, atomic browser refresh replacement
and revocation tombstones. Existing clients and API-key routes remain compatible.
`go test ./...`, `go build ./...`, `go vet ./...` passed; live Firestore concurrency
and production login were not tested. Browser session tombstones contain a hash
identifier and timestamp, never page evidence or URLs.

Dashboard: separate `/browser/connect`, existing login return-to handling, exact
callback and state verification. Full suite 32 tests and production build passed.
This branch builds on the existing mobile-auth dashboard branch; reconcile that
base with the release branch before deployment.

Android: native first-launch pairing, secure session/pending vaults, single-flight
renewal, logout and account/developer credential separation. Analysis waits for
refresh within its existing 10/20-second deadline and retries authentication at
most once; cache/settings generations reject results from an old account.
Browser OAuth assertions: 19 passed. Branding text tests: 5 passed. Shared contract:
8 passed. Vault instrumentation: 23 assertions passed in two emulator processes
(prepare PID 2877, verify PID 2908), including encrypted persistence, key-purpose
isolation, tamper rejection and independent deletion. Native source C++/Java API
compilation passed. Account/branding x64 APK and AAB built successfully, including
bundle manifest/dex sanity checks after placing the callback in the chrome split.
The APK installed and launched on API 35 x86_64; launcher, welcome and native
account onboarding were visually checked. Live account login, refresh/revocation
and all UI surfaces still require acceptance with compatible deployed services.

Android pairing uses an external installed browser and excludes PhiShark itself,
including when PhiShark is the default browser. With no external browser it shows
a setup error. Network/refresh response loss may require a fresh login; a permanent
session is not promised. No real account token or production endpoint was used in
these tests. Login/content consent are separate, and the API-key fallback is under
About → developer settings.

Pushed implementation: backend `725fe1b` ([PR 21](https://github.com/phishark-git/b-backend-service/pull/21)),
dashboard `fb05b89` ([PR 29](https://github.com/phishark-git/w-phishark-dashboard-website/pull/29)),
browser native account/branding `ad710424`, factory new-tab suggestions `8308008b`.
No production workflow was triggered. Graph code/components/workspace were
refreshed and both original impact queries repeated; semantic docs and other
branch snapshots remain incomplete, as described in the graph impact report.

iOS: `BrowserAccountFlow` and purpose-specific Keychain storage were added to the
standalone Swift package. They are uncompiled on Windows. The Mac handoff describes
ASWebAuthenticationSession, token exchange/refresh and branding integration after
the unchanged Fennec baseline. No working iOS login is claimed.

## Login failure follow-up

The generic start error hid HTTP failures behind a connectivity message. Android
now distinguishes missing/unsupported routes (404/405), throttling (429), server
errors, timeout, connection and invalid-response/storage failures without showing
response bodies or account secrets. The public API contract is unchanged.
The dashboard pairing branch is reconciled with current main while preserving
both browser pairing and the existing deep-scan route. Implementation order is
dashboard conflict resolution and Android diagnostic copy; deployment order and
the separate production approval gate remain unchanged.

GitHub evidence on 2026-10-09: the last successful official backend workflow
run `36936617829` used `9d6cc566`, whose source has no `/browser/auth` routes;
dashboard run `36941978828` used `2d46213d`, whose source has no
`/browser/connect` route. This proves the new routes are absent from those
workflow revisions, not the exact live HTTP response or unobserved runtime
changes. No production request or VDS inspection was performed. Dashboard
reconciliation `f0b17e6` passed 86 tests, serve-config validation and build;
PR 29 is mergeable. Backend PRs 20/21 remain open with successful CI.
