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
