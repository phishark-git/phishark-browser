# Local mobile acceptance fixtures

Run `npm run fixtures` on the development host. It listens on loopback port 8765
and makes no outbound request. Use the synthetic API key `fixture-only`, never a
personal production key or real page evidence. A connected Android device can
use an explicitly configured development build plus `adb reverse tcp:8765
tcp:8765`; a Mac simulator can run the same fixture server locally. Production
networking must stay HTTPS-only; loopback HTTP is a test exception, not release
configuration. The task-local Windows emulator has installed baseline comparison
packages and launched the locally built x64 baseline; actual PhiShark navigation
preflight/deep blocking, return-to-safety and private URL-only smoke tests passed
against the corrected integrated x64 APK. The wider acceptance suite remains pending.

## Leaving fixture mode before account acceptance

Fixture-capable APKs enter synthetic mode only when the Android command-line file
contains `--phishark-local-fixtures`. In this mode public targets are deliberately
not scanned, and only loopback fixtures use the synthetic credential/consent.
Installing a newer APK with `adb install -r` does **not** clear this command-line
file. Before live account acceptance, remove only this switch from
`/data/local/tmp/chrome-command-line`, preserve unrelated switches, verify its
absence, and restart the app process. Do not clear app data or the account vault.
Do not print the whole command line or any stored credentials while diagnosing.

On 2026-10-09 the emulator still had this flag after the account build was
installed. A signed-in public page therefore showed unverified before an API
request could run. The flag was removed and the app restarted without clearing
data. A subsequent reload reached a low-risk URL result. Later unverified
states require their own diagnosis; a successful preflight is not proof of a
successful deep scan. The native panel now includes fixed, credential-free
failure details instead of conflating local fixture exclusions, connection
validation, capture failures, HTTP errors and partial analysis.

Open `http://127.0.0.1:8765/`. Pages cover score/prompt/degraded/error scenarios,
two-hop redirects, popup/new tab, same-document history and anchors, downloads,
file inputs, scroll-offset masking, editable text, a frame and shadow fields.
Synthetic form values contain the `fixture-private-` marker. The fixture API
rejects leaked markers, Cookie/Authorization headers/evidence and preflight
evidence. A server-side rejection does not prove screenshot masking: inspect
actual captures visually, including input pixels/scroll positions. Add a
separate-origin frame fixture for cross-origin acceptance on the device.

The fake API uses the real ephemeral paths/envelope and returns deterministic
decisions, one capacity retry, quota/auth/configuration/transient failures,
malformed JSON or a stalled response body. `/stats` reports numeric counts,
including page GET counters indexed only by known synthetic scenario names and
redirect check/request totals. It can demonstrate that a preflight block
prevented the document GET; no target string is retained.
There are no request/payload/URL logs or scan records; retry bookkeeping is an
in-memory digest and disappears when the fixture closes. No real model,
decision-maker, database, threat feed or production privacy claim is involved.

`npm test` exercises the real shared HTTP transport against these fixtures,
including body-read deadline/cancellation and private deep rejection. This
does not run a Chromium/WebKit engine. Complete the native
[device acceptance record](device-acceptance.md) once baseline launch and native
integration are available.
