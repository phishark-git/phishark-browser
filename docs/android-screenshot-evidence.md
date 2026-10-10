# Android HTML and masked PNG evidence

## Implementation plan and compatibility

The Android browser is the changed consumer. Backend browser-deep already accepts
`web_evidence.response.html` and `.screenshot`; orchestrator selects HTML for
`m-llm-content-analysis` and PNG for `m-vlm-analysis`. Links/favicon consume the
same sanitized page evidence, and Decision Maker consumes their findings. No
provider/model/key, callback, persistence, quota or public route migration is
needed. Orchestrator is also changed: ephemeral mobile browser evidence must
always use module-specific projections even when the global
`SELECTIVE_WEB_PAYLOADS_ENABLED` flag is off. Its non-JSON request-local marker
must not be controllable by public callers. Extension and persistent Playwright
profiles retain their existing global-flag behavior and contracts.
Production feature flags and external model retention remain unknown.

Implementation order: verify providers' current contracts and fix scoped evidence
projection in orchestrator; add native Android
capture and pixel redaction; validate native build and local device transport;
update Mac handoff and graphs. Deployment order: orchestrator's documented
`deploy-self-hosted.yml` after explicit approval, then the validated Android APK.
Backend and individual analyzers need no deployment. Local APK/fixture validation
does not depend on a production deployment. Callbacks/history remain suppressed
for mobile scans, and the legacy persistent routes retain their behavior.

The browser uses a private, temporary in-process DevTools client. It does not
enable a network debugging port or expose credentials/commands to page scripts.
DOMSnapshot supplies layout, including flattened open/closed shadow trees.
After a document visual-state barrier, the browser reads the native compositor
surface without changing device emulation or page dimensions. Android frame
metadata crops out browser-control reservations using current device density
and page scale; no fixed screen dimensions or toolbar pixel offsets are used.
The bitmap dimensions come from the native compositor. Frame geometry is rechecked
before and after capture. Blink DOMSnapshot bounds are physical layout pixels,
not CSS pixels: they are normalized using LayoutZoomFactor from Page's physical/
CSS viewport ratio. Mask coordinates then use that conversion plus pinch zoom;
cropping does not stretch masks to the output image dimensions. This distinction
is essential on devices with density other than 160 dpi.
`Page.captureScreenshot` is deliberately not used: device
testing showed it could change the Android viewport during capture.
Native pixel redaction covers inputs, textareas, selects, editable/ARIA controls,
frames, custom elements and their laid-out descendants. Only the visible web
viewport is captured, excluding browser/account UI. Capture is bounded and
cancelled with navigation/tab destruction. Document identity, viewport and mask
geometry are checked again after capture; unstable or failed capture falls back
to partial HTML and stays unverified. Normal consent is required; private mode
never captures either HTML or pixels. Raw snapshots and image buffers stay in
memory and are never logged or written by the production browser.
The normal-mode consent text explicitly includes the masked screenshot.

HTML remains sanitized; this is not an upload of cookies, headers, form values
or every frame's document. Masks cannot remove arbitrary sensitive text that a
site has copied into unrelated page content or images. Post-load analysis cannot
undo executed JavaScript. Pixel coverage and device limits must be reported from
actual validation, separately from source implementation.

## Validation

Implemented and built against the pinned Chromium/Cromite tree on 2026-10-10.
The final x64 fixture-capable, development-signed APK and AAB build completed in
2m50.20s after baseline incremental reuse. Device tests used the isolated
`phishark_capture_api35` emulator, with 420×840/160 dpi and 600×1000/240 dpi
Android display configurations. These are two emulator configurations, not two
physical phones. Normal-mode consent explicitly includes masked pixels.

Browser validation: `npm test` 12/12; native C++ decision/navigation vectors
58/58; Java OAuth assertions 19/19 and Python branding tests 5/5 passed.
Orchestrator validation on `60b2ceb`: `go test ./...`, `go build ./...`,
`go vet ./...` passed, and PR #25 test/security CI passed. Its actual HTTP fan-out
fixtures verify exclusive HTML/PNG payloads and retained no-persistence/callback
behavior with the global projection flag disabled. Unchanged backend, LLM and
VLM repositories' own Go suites passed separately; no production/model endpoint
was used for these tests.

The synthetic HTTP receiver decoded the actual Android deep request's PNG, checked
every supplied region for opaque black pixels, independently counted magenta
sensitive-control canaries and blue public branding, and rejected unsafe trials.
Final build results (one preflight + one deep per fresh normal navigation):

| Display configuration / page | Received PNG | Masks | Magenta pixels | Blue pixels |
| --- | --- | --- | --- | --- |
| 420×840, 160 dpi, normal | 420×744, 11,811 bytes | 16 | 0 | 15,387 |
| 420×840, 160 dpi, 1.5× zoom | 420×736, 16,110 bytes | 16 | 0 | 22,275 |
| 600×1000, 240 dpi, 1.5× zoom | 594×834, 17,319 bytes | 16 | 0 | 47,153 |
| 600×1000, 240 dpi, scroll Y≈742.93 CSS px | 594×834, 12,351 bytes | 16 | 0 | 31,740 |

Received PNGs were also inspected visually. HTML/title/target matched the fixture;
script/frame documents and synthetic form-value markers were absent. Final suite
privacy rejections: zero. Moving controls produced HTML-only evidence with no PNG
(geometry changed at step 5); the browser keeps incomplete safe results unverified.
Whitelist produced 1 preflight / 0 deep; blacklist produced 1 / 0 and prevented
the document GET, displaying the native block with no bypass. Private mode added
1 URL preflight / 0 deep, sending neither HTML nor PNG. With fixture mode disabled,
the same loopback page loaded with 0 additional preflight/deep and no block dialog.

Earlier trials exposed CDP viewport changes, an incorrect toolbar-origin crop,
missing pinch-scale conversion and physical-layout pixels misread as CSS at
240 dpi. Unsafe image trials were sent only to the synthetic loopback receiver,
which rejected their canaries; they were not production/model requests. The live
emulator was rolled back to the previous HTML-only APK while capture was being
fixed. The physical/CSS normalization above fixed the final density and zoom
cases. These fixtures prove targeted Android capture/transport behavior, not
universal privacy across arbitrary sites or all hardware.

Physical ARM launch, iOS capture, production projection rollout and complete MVP
readiness remain unverified. New orchestrator PR #25 is not deployed. No hosted
model or production endpoint was invoked for these acceptance fixtures.

The original live-browsing emulator's fixture switch was restored to off; its
account and app data were preserved. See [mode separation](local-device-fixtures.md).
Synthetic fixture PNGs and numeric receiver checkpoints are stored only in the
ignored `.build/screenshot-evidence/` and `.build/screenshot-final-*-stats.json`.
The production browser never writes captured scan images to disk. Remaining
navigation, cancellation, cross-origin and real-device coverage belongs in the
wider acceptance suite; passing contract/unit tests are not substitutes for it.

## Local handoff

Build source: `f6810926`, pushed to `codex/browser-mvp`. The verified APK was
installed with `adb install -r` on the original `emulator-5554` and launched;
existing account/app data and tabs were preserved. Its fixture flag was verified
absent after boot. The isolated test emulator's flag was also restored to off,
its reverse removed, its process closed and the owned fixture server stopped.

Development artifacts are kept in ignored `.build/artifacts/native-screenshot-20261010/`:

- APK SHA-256: `d3223b0f5d87082db3c6fb836cae663a90eeda9aacd350d04d585e1789fe1d6e`.
- AAB SHA-256: `4f5f45f89c7af7a1c9ed6d4c9b6b7dffadf8003922585a4a93cd045e8cc96dc9`.

These are x64 development artifacts, not store-ready ARM64 releases. The local
manifest binds artifacts to the tested source. Production evidence projection
still requires review/approval and the official orchestrator workflow for PR #25.

## Inspecting blocked live results

The native risk dialog now opens the existing account/protection panel without
resuming the blocked navigation. This exposes the current result phase and
navigation-local request/cache/retry counters even while the app menu is covered.
No API, provider, persistence, capture or decision policy changes are involved.

A user-requested live normal-mode check returned score 100 and a preflight block.
The panel showed URL POSTs 1, content POSTs 0, cache hits 0 and retries 0, with the
account connected. Consequently this particular navigation sent no HTML/PNG and
did not produce a deep score. A preflight block is not deep/VLM acceptance proof.
The requested target is intentionally not recorded in this public document.
Validation: x64 APK/AAB build 4m12.63s; in-place installation and panel read passed;
`npm test` 12/12, OAuth assertions 19/19, branding checks 5/5 passed. Native capture
and backend code are unchanged, so earlier pixel tests retain their original scope.

## Live upload inspection

The protection panel now reports UTF-8 HTML bytes and decoded PNG bytes from the
exact serialized JSON attached to each dispatched deep POST. PNG size is derived
from the native encoder's standard padded base64. It also reports the latest deep
HTTP status and Chromium network completion code. These are navigation-local,
memory-only numbers; captured content, URLs and credentials are not diagnostic
data. A new navigation clears them. Cached deep results do not claim an upload.
Retries retain their explicit POST counters and report the latest attempt.

This proves that fields are attached to the browser request and whether the deep
endpoint replied successfully. It does not independently prove which downstream
model consumed the evidence, server retention policy, or universal masking.
The earlier independent synthetic receiver/pixel checks retain their own scope.
No backend/provider schema, callback, persistence or workflow is changed.

Live checks on 2026-10-10 used the connected account on the original API 35 x64
emulator, with fixture mode verified off. The requested targets are deliberately
excluded from this public report:

| Requested navigation | Phase / result | URL / deep POSTs | HTML / PNG bytes | Deep response |
| --- | --- | --- | --- | --- |
| Benign page | Deep, safe, score 15 | 1 / 1 | 34,061 / 104,959 | HTTP 200, network 0 |
| Threat target | Preflight, blocked, score 100 | 1 / 0 | No deep upload | Not applicable |

Both final navigations had zero cache hits and zero authentication/capacity
retries. The threat navigation cleared the previous page's deep diagnostics.
An earlier benign navigation on the preceding APK also returned deep score 15;
the fresh navigation after installing the diagnostic APK was an intentional
manual retest, not a duplicate automatic POST. The app's HTML/PNG evidence was
not copied to logs, disk or this repository for these real targets.

Validation: x64 APK/AAB build succeeded in 4m35.21s; in-place APK installation,
launch, live deep upload/response and native preflight block/panel reads passed.
`npm test` 12/12, native decision vectors/invariants 58, OAuth assertions 19 and
branding tests 5/5 passed independently. APK/AAB snapshots and their manifest are
in ignored `.build/artifacts/live-evidence-20261010/`. These remain development
x64 artifacts; physical ARM/iOS and downstream VLM execution are not established
by this browser-side inspection. No deployment or VDS inspection was performed.
