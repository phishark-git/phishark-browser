# Implementation status

Last reviewed: 2026-10-09. Status must be updated from actual command results.

Android `9c6cbe6d`: one static English indicator spans preflight, document loading
and deep; English default resources cover PhiShark-specific UI. The actual x64
build and targeted emulator handoff/block checks passed with unchanged request
counts. [Validation and limits](android-continuous-scan-ui.md). Mac handoff updated;
iOS acceptance remains independent and pending.

Android `d1aebded`: per-navigation API attempt/cache/retry counters and distinct
URL/content wait labels compiled, built and passed targeted emulator checks.
Panel counts matched synthetic server counts, including same-document events,
capacity retries and cached reload. Stalled responses completed as unverified
with the indicator cleared. See [validation and limits](android-request-observability.md).
This does not prove live production request totals or iOS behavior.

[Production rollout](production-rollout-20261009.md): all 11 official deployment
runs succeeded, followed by 5/5 public smoke checks, including a valid synthetic
S256 PKCE login start. Human account login/refresh and authenticated scan privacy
acceptance remain unverified.

Android follow-up: the user completed login; the emulator retained the account
through app restarts and in-place updates. A leftover local-fixture runtime
switch was removed. The x64 `326ae22f` build now preserves the URL-only result
when content verification is incomplete; the real page displayed low URL risk
with partial coverage. This does not establish successful deep analysis or refresh
acceptance. See [runtime diagnosis](android-unverified-followup-20261009.md).

Latest Android correction `bd7885ee`: the same-document repetition test changed
from 1 preflight + 6 deep POSTs to 1 + 1, including an in-flight deep response.
Normal browsing now has no floating status badge; the app menu exposes account
and protection details. Query changes, redirects and native blocks were exercised
on the updated x64 emulator. [Measured evidence](android-request-deduplication.md).

Active-scan UI `6f9f1da1`: a small wait indicator now appears only while the
selected tab is being checked; completed results remain quiet. The x64 APK/AAB
built and the indicator's appearance, completion and navigation-away behavior
were observed on the emulator. Fixture request counts stayed at one preflight
and one deep per new check. [Validation](android-scanning-indicator.md).

| Gate | Status |
| --- | --- |
| Playwright implementation inspected | Complete; main and prompt-policy branches distinguished |
| Public repository | Created; implementation on codex/browser-mvp |
| Upstream source imports | Complete; pinned Cromite and Firefox iOS subtrees; latest-release checks passed |
| Shared contract and vectors | JS policy/client/cache/session implemented; tests passed; Android integration has partial device evidence; iOS pending |
| Ephemeral orchestrator/backend | Separate Go tests/builds passed; orchestrator PR 23 and backend PRs 20/21 merged and official production workflows succeeded. Live authenticated scan/retention acceptance remains pending; see production rollout report |
| Shared-provider privacy prerequisites | Seven provider changes tested, merged and deployed by official workflows; application/proxy/provider retention acceptance remains separate |
| Native Android policy | C++ compiled in WSL; shared vectors and session invariants passed |
| Secure key storage | Android vault passed 23 emulator assertions across two processes, including separate session/pending/key storage. Swift Keychain purposes prepared, Mac tests pending |
| Account onboarding | Scoped browser auth deployed; dashboard pairing passed 86 tests/build. Android x64 APK/AAB built, installed and opened; welcome/account UI visually checked. Public route checks are recorded in the production rollout report; human login/renewal/device acceptance remains pending. Swift flow helper prepared; iOS account adapter continues from the completed Mac simulator baseline |
| Unmodified Android baseline | ARM64 APK/AAB built; ARM64 emulator launch failed including official comparison. Local x64 APK/AAB built, installed and opened the local fixture page successfully |
| Unmodified iOS baseline | Mac branch `9064f050` reports unchanged Fennec build/launch and integrated Fennec build/launch passed on iPhone 17 Pro simulator using Xcode 26.6 experiment mode. Pinned Xcode 26.5 and physical iPhone remain unverified |
| Native navigation integration | Corrected x64 APK/AAB built and basic security smoke checks passed. Integrated ARM64 APK/AAB (pre-account overlay) built in 44m38s; physical launch unverified. New x64 account/branding APK/AAB built; wider Android acceptance and iOS integration pending |
| Release signing/device/store acceptance | Pending |
| Production provider deployment | Approved and official provider/orchestrator/backend workflows succeeded; see production rollout report for exact commits, workflow runs and endpoint evidence. Flags not exercised by a live authenticated scan remain unknown |
| Cross-repository graph refresh | Affected code/component graphs refreshed; original impact query repeated; document semantics/other branch snapshots remain incomplete |

A development-only x64 PhiShark APK/AAB has been produced and the APK launched.
Its basic native security smoke checks passed after correcting the initial
firewall failure; full acceptance is incomplete. Integrated ARM64 compile has passed;
no physical ARM64 launch or signed release is claimed. The separate Mac branch
`codex/ios-mac-validation` records baseline/integrated simulator builds; Windows
did not repeat these. iOS account onboarding and physical-device acceptance
continue on Mac using [the account handoff](../ios/MAC_ACCOUNT_ONBOARDING.md).

The MVP is complete only when both native builds, security/privacy tests, device
acceptance and everyday browser function checks have passed. A contract package or
source import is not a mobile browser release.

Evidence, PRs and remaining gates: [validation report](validation-report.md),
[graph impact](graph-impact-report.md), [device acceptance](device-acceptance.md).
