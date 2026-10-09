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
