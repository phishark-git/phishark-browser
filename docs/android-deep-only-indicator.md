# Deep-only scanning indicator and evidence audit

The user's current preference supersedes the preflight-to-deep continuous
indicator: show one static English label only after an actual deep request starts.
Preflight, waiting for document load, evidence preparation and cache hits stay
quiet. Keep the label through bounded transport retries until completion, error,
cancellation or selected-tab change; terminal block/warning UI uses existing rules.

Impact query: `PhiSharkBridge refreshScanningIndicator setDeepPending Captured
web_evidence screenshot`. Verified against SYSTEM_MAP.yaml and current source.
Provider: Android browser-process deep dispatch; consumer: JNI PhiShark UI.
Evidence providers: renderer isolated-world HTML snapshot → native browser request
→ backend ephemeral route → orchestrator analyzer projections. No server schema,
policy, persistence, callback or provider/model setting change is introduced.
Implementation order: native dispatch timing and Java visibility → fixture audit
metadata → tests/build/device checks → Mac handoff. Deployment order: verified
mobile clients only; no server deployment is required. iOS independently pending.

## Evidence actually sent

Current Android request is `{target, web_evidence: {response: {html, url},
capture_coverage: "partial_html_no_screenshot"}}`. The HTML is a bounded clone of
the loaded document, not a fabricated server response. Scripts/styles/templates,
form values, editable contents, frames and custom control contents are removed;
event/data/value attributes are stripped. Cookies/authentication headers are not
page evidence. Native account credentials remain outside the renderer.

**No screenshot is captured or sent.** A native screenshot with verified pixel
masking for forms, frames, shadow/custom controls, scaling and scrolling is still
missing. The VLM projection requires `response.screenshot` or screenshot artifact;
HTML alone cannot supply visual analysis. Outgoing-link metadata, page/header
statistics and observed redirect-chain evidence are also not present in the
mobile payload. Optional support in an orchestrator validator does not mean all
analyzers have complete evidence. Partial deep results must not certify safety;
the Android client retains its partial-result unverified behavior.

The local synthetic receiver records only byte counts/presence flags and checks
for the expected fixture title, matching response URL, absent script/frame markup
and sensitive-marker rejection. It does not save HTML, pixels, target URLs or
credentials. This proves local native dispatch; it is not a live production
provider/retention verification or proof of screenshot masking.

## Validation

Tests, final APK/AAB build and targeted emulator evidence are appended after
execution. Full screenshot/complete-evidence acceptance remains pending, and this
UI change does not imply that acceptance. Existing Graphify mixed branch, old-ID
and incomplete semantic coverage limitations remain.
