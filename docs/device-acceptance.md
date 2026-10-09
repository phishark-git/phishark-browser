# Native/device acceptance record

Every entry below is **pending**. Record platform, upstream/build commit, toolchain,
device/OS, fixture, expected/observed behavior and a local evidence reference.
Do not use server/JS tests as proof of native device behavior.

Use the [local fixture harness](local-device-fixtures.md) for deterministic
navigation/API/privacy inputs. It is ready; native execution against it is pending.

| Acceptance group | Required behavior |
| --- | --- |
| Baseline | Original Cromite ARM64 APK and AAB build; original Fennec simulator build and device launch; record signing separately without credentials. |
| Navigation | Main document, every available redirect hook, history/back/forward, same-document, restore, BFCache/prerender where applicable, popup/new tab, intent/deep link; exact tab/navigation generation; stale/cancelled results cannot alter a newer page. |
| Verdict boundaries | Both sides of 30/31, 60/61, 85/86; definite threat and suspicious/malicious prompt block without bypass; deep uses existing fusion score once; negative/non-finite/malformed/degraded results stay unverified. |
| Error and time budget | Offline/timeout/body stall/invalid envelope; full 10/20-second deadline; at most one capacity-429 retry within budget; auth/quota/setup failures use service-error UI; no persistent endpoint fallback. |
| Consent/private | Explain full URL/query and page analysis before normal automatic deep; denial prevents capture; private always URL-only; private cache/session destroyed on closure and cannot repopulate. |
| Evidence privacy | Exclude cookies, authorization and input/textarea/select/editable values; screenshot masks at correct scale/scroll position, frames, shadow DOM and custom controls; unsafe capture omitted with unverified state. |
| Credentials | Keystore/Keychain round trip, removal, locked device, reinstall/backup behavior; native code only; renderer/devtools/JS bridge cannot read the API key; no secret-bearing diagnostics. |
| Native UI | Checking/safe/warning/blocked/unverified; deep warning override for this exact navigation only; blocked has no override; return to last safe page or new tab; security pages excluded from scanning. |
| Core browser functions | Multiple tabs, private tabs, history, bookmarks, downloads, permissions, file uploads, sharing, find/zoom, normal navigation and recovery; no engine replacement. |
| Network/storage privacy | Inspect synthetic payloads and all participating application/proxy/APM logs; no scan/history/evidence/callback writes; only numeric usage accounting; verify memory cache TTLs, URL+evidence-hash keys, isolation and cancellation. |
| Platform differences | WebKit intermediate redirect requests may escape pre-request interception; record fixtures and observed limits. Post-load blocking cannot undo prior JavaScript on either platform. |
| Release | `io.phishark.browser`, PhiShark marks, upstream notices/source obligations, telemetry/sync/updater audit, signed ARM64 APK/AAB and iOS release, current store requirements/privacy declarations; no store upload until approved. |

A known provider compatibility case to verify: VLM's inline screenshot parser
expects bare base64 rather than a `data:` URI. The current fixture uses bare
base64. Normalize native capture to the supported contract and test actual image
decoding; do not silently count a rejected VLM payload as successful deep analysis.

MVP completion requires both platforms, security/privacy checks and everyday
function acceptance. Mark failures and unsupported paths explicitly; do not
replace an observed gap with an inferred guarantee.
