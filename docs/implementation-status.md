# Implementation status

Last reviewed: 2026-10-09. Status must be updated from actual command results.

| Gate | Status |
| --- | --- |
| Playwright implementation inspected | Complete; main and prompt-policy branches distinguished |
| Public repository | Created; implementation on codex/browser-mvp |
| Upstream source imports | Complete; pinned Cromite and Firefox iOS subtrees; latest-release checks passed |
| Shared contract and vectors | JS policy/client/cache/session implemented; tests passed; Android integration has partial device evidence; iOS pending |
| Ephemeral orchestrator/backend | Implemented in isolated worktrees; separate Go tests/builds passed; deployment pending approval |
| Shared-provider privacy prerequisites | Seven isolated provider changes tested; nine server/provider draft PRs available; see validation report |
| Native Android policy | C++ compiled in WSL; shared vectors and session invariants passed |
| Secure key storage | Android vault passed 23 emulator assertions across two processes, including separate session/pending/key storage. Swift Keychain purposes prepared, Mac tests pending |
| Account onboarding | Scoped backend browser auth and dashboard pairing pushed; Go tests/build/vet and 32 dashboard tests/build passed. Android x64 APK/AAB built, installed and opened; welcome/account UI visually checked. Live account login/renewal pending deployment and device acceptance. Swift flow helper prepared; iOS adapter pending Mac baseline |
| Unmodified Android baseline | ARM64 APK/AAB built; ARM64 emulator launch failed including official comparison. Local x64 APK/AAB built, installed and opened the local fixture page successfully |
| Unmodified iOS baseline | Pending; user's MacBook required |
| Native navigation integration | Corrected x64 APK/AAB built and basic security smoke checks passed. Integrated ARM64 APK/AAB (pre-account overlay) built in 44m38s; physical launch unverified. New x64 account/branding APK/AAB built; wider Android acceptance and iOS integration pending |
| Release signing/device/store acceptance | Pending |
| Production provider deployment | Not approved; runtime flags unknown |
| Cross-repository graph refresh | Affected code/component graphs refreshed; original impact query repeated; document semantics/other branch snapshots remain incomplete |

A development-only x64 PhiShark APK/AAB has been produced and the APK launched.
Its basic native security smoke checks passed after correcting the initial
firewall failure; full acceptance is incomplete. Integrated ARM64 compile has passed;
no physical ARM64 launch, signed release or iOS build is claimed.
iOS needs the MacBook.

The MVP is complete only when both native builds, security/privacy tests, device
acceptance and everyday browser function checks have passed. A contract package or
source import is not a mobile browser release.

Evidence, PRs and remaining gates: [validation report](validation-report.md),
[graph impact](graph-impact-report.md), [device acceptance](device-acceptance.md).
