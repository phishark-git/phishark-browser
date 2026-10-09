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

Source `12ffd871`: 11 JS tests, 58 C++ vectors/navigation invariants, 19 OAuth
assertions and 5 branding tests passed. Actual Chromium C++/Java/JNI/resources
and x64 APK/AAB packaging passed in 4m08.90s. The existing upstream XR
`UsedByNative` warning was nonfatal. In-place APK installation preserved app data.

The synthetic handoff fixture delays preflight 1 second, document response 1.5
seconds and deep response 3 seconds. Sampled screenshots after Enter:

| Sample | Indicator |
| --- | --- |
| 650 ms, preflight | Hidden |
| 1,714 ms, document waiting | Hidden |
| 3,305 ms, deep | Visible, static English label |
| 4,302 ms, deep | Same visible static label |
| 6,515 ms, completed | Hidden |

This is sampled UI evidence, not a frame-by-frame recording. Receiver counters
were exactly 1 preflight / 1 deep. Native evidence contained 2,069 HTML bytes;
the expected fixture title and full response URL matched. Script/frame markup
was absent, sensitive fixture markers caused zero privacy rejections, and there
were 0 screenshot bytes (`hasScreenshot: false`). Coverage explicitly remained
`partial_html_no_screenshot`. No actual HTML, target URL or credentials were
retained in the audit metadata. This verifies genuine loaded-page HTML dispatch
while proving screenshot capture is still missing.

Returned to new tab, restored original command line and verified fixture mode
off; local server stopped. Normal launch was repeated. APK SHA-256:
`c8349f0777b82aba715237dd07cd4a85115aa200e81a15189175c10dfd6f881d`.
AAB SHA-256:
`aa3bef1a6cc5c3e51a3b5711ae8c1a2cf729fc4fcde6f9291cc97beb62a93112`.

This run did not add native device checks for every retry, deadline, background
tab, private session, SPA/restore path or live provider request. Full screenshot,
complete-evidence acceptance and Mac adapter checks remain pending; this UI
change does not imply that acceptance. Server repositories were inspected but
unchanged and not retested as evidence for mobile behavior.

Browser code graph updated: 586 nodes / 1,203 raw edges; browser component 1,044
edges. Workspace rebuilt: 14,144 nodes / 30,138 edges, zero dangling aggregate
endpoints. Original impact query repeated (233 nodes, budget truncated). Mixed
branches, older IDs, four conceptual nodes missing source paths and incomplete
Markdown/XML semantic coverage still limit graph completeness. No server rollout
or compatibility migration is required. Mac UI/evidence acceptance remains the
explicit unresolved platform consumer.
