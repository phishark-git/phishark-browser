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
compilation passed; the new account/branding APK build and visual acceptance are
separate from these checks.

Android pairing uses an external installed browser and excludes PhiShark itself,
including when PhiShark is the default browser. With no external browser it shows
a setup error. Network/refresh response loss may require a fresh login; a permanent
session is not promised. No real account token or production endpoint was used in
these tests. Login/content consent are separate, and the API-key fallback is under
About → developer settings.

iOS: `BrowserAccountFlow` and purpose-specific Keychain storage were added to the
standalone Swift package. They are uncompiled on Windows. The Mac handoff describes
ASWebAuthenticationSession, token exchange/refresh and branding integration after
the unchanged Fennec baseline. No working iOS login is claimed.
