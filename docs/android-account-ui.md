# Android account screen

The app-menu entry is now **PhiShark account**, opening the same native screen
as the first account row in Settings. It displays the signed-in user's name and
email and a direct Sign out action. The existing browser settings, site permission
controls and normal notification settings remain available. No promotional or
new system notifications are introduced.

Normal account UI and native risk dialogs no longer show scan phase, preflight,
scores, request/cache/retry counts, HTTP/network codes or HTML/PNG sizes. Numeric
diagnostics remain native memory state; the live-evidence report records their
earlier verification. The quiet deep-only scanning indicator retains the previous
explicit preference. No scan decision, blocking threshold or evidence policy is
changed by the account UI.

Profile fields already exist in the backend's token/refresh response `data.user`.
Android stores only `first_name`, `last_name` and `email` alongside the session in
its encrypted no-backup vault. Tokens never leave the native account owner.
An existing session without profile fields can use the existing bounded refresh
operation to populate them, retaining the current credential until replacement
is saved. It does not request a dashboard-wide scope or a new profile endpoint.
Fields are bounded and stripped of control/bidi formatting characters. Missing
identity stays unavailable; there is no fabricated name, plan, avatar or usage.

First sign-in still requires separate, explicit consent to transmit page content.
The page-protection preference explains full URLs including queries, sanitized
HTML and masked screenshots, with URL-only incognito. Opening the account page
does not toggle consent. Sign out keeps the existing immediate local clearing
and best-effort server revocation, removes profile/pending credentials and clears
consent. Listener lifecycle cleanup prevents a destroyed account Activity from
remaining registered. No web renderer receives account fields or credentials;
the native account Activity excludes Android screenshots/recents via FLAG_SECURE.

## Impact and validation

Implementation order: Android account persistence/UI, pinned upstream Settings
overlay, local APK build, isolated native profile tests and device UI inspection.
The existing backend token response is the provider; no backend, orchestrator,
model, public scan API, callback or deployment workflow change is needed. There
is no server deployment for this work. Mac UI parity is a documented handoff,
not verified Swift implementation.

The isolated account-test APK has no Internet permission, launcher or access to
the real browser's UID. It compiles the actual BrowserAccount/ApiKeyVault classes
with a test-only ContextUtils adapter. Two instrumentation processes passed 18
assertions: selected fields only, encrypted persistence/reload, refresh without
user data, malformed display fields, no identity inherited by a different login,
and the same native clearing path used by Sign out. It never signs out the real
user or contacts an auth service.

Final x64 APK/AAB build passed in 3m41.95s. An initial resource rebuild stopped
on a generated file with a future timestamp; a normal rerun passed without
changing host clock or clearing build caches. The same nonfatal upstream XR
UsedByNative warning appeared as in earlier builds. Source supports the pinned
API 29 minimum; visual device checks were API 35 only.

In-place installation preserved the real user's account, tabs and consent.
The app-menu profile screen and Settings' first account row were inspected; the
existing auth refresh supplied actual profile fields, and they survived another
APK update/restart. The final profile screen has one heading, the PhiShark mark,
name/email, page-protection preference and a direct Sign out button. No live-user
Sign out or consent change was executed. The account-menu icon was verified as
the existing native account glyph rather than a tinted opaque logo square.
Only a leftover command file containing exactly `chrome` was removed from the
test emulator; custom flags would have been preserved. The final launch had no
test-command-file warning, and fixture mode remained off.

`npm test` passed 12/12; the OAuth assertions passed 19 and branding checks 5/5.
Native profile minimization/clearing checks passed 18 in their isolated app UID.
These are independent checks, not proof of iOS, arbitrary navigation, server
revocation or store readiness. The development APK/AAB and numeric manifest are
kept in ignored `.build/artifacts/account-ui-20261010/`; no real identity fields,
screen pixels, account tokens or live-page evidence are stored in this report.

Reproduce with the existing pinned Chromium build tools:

```sh
python3 android/scripts/build-vault-tests.py /Linux/build-root/src --account-profile
adb install -r .build/account-profile-tests/PhiSharkAccountProfileTests.apk
adb shell am instrument -w -e phase prepare io.phishark.browser.accounttests/.AccountProfileInstrumentation
adb shell am instrument -w -e phase verify io.phishark.browser.accounttests/.AccountProfileInstrumentation
```

The browser's physical ARM64/iOS builds, full navigation/privacy acceptance,
downstream VLM execution, release signing and store acceptance remain separate
gates. This UI change does not complete the mobile MVP.
