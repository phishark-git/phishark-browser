# Validation and delivery evidence

Reviewed 2026-10-09. This is an implementation foundation, not an accepted mobile
MVP. No production workflow or store publication was triggered.

## Repository delivery

Public browser repository: [phishark-browser](https://github.com/phishark-git/phishark-browser),
branch `codex/browser-mvp`. Cromite source pin `49c23e54`, Firefox iOS pin
`fbb79fe3`. Chromium source remains outside this repository on the WSL filesystem.

Each existing repository was changed in a separate worktree. Original dirty
checkouts and unrelated user changes were preserved. Server branch:
`codex/browser-ephemeral-20261009`; provider branch:
`codex/browser-log-privacy-20261009`.

| Repository | Source base | Tested implementation | Draft review |
| --- | --- | --- | --- |
| backend-service | `6df894b` | `219c3fd` | [PR #21](https://github.com/phishark-git/b-backend-service/pull/21), stacked on [usage PR #20](https://github.com/phishark-git/b-backend-service/pull/20) |
| scan-api-orchestrator | `b297500` (same tree as fetched main) | `76ad103` | [PR #23](https://github.com/phishark-git/o-scan-api-orchestrator/pull/23) |
| g-gatekeeper | `e22d866` | `207931b6` | [PR #2](https://github.com/phishark-git/g-gatekeeper/pull/2) |
| m-domain-similarity-analysis | `4d724e5` | `7b7fd2f` | [PR #2](https://github.com/phishark-git/m-domain-similarity-analysis/pull/2) |
| m-llm-content-analysis | `20e8635` | `395f4a7` | [PR #3](https://github.com/phishark-git/m-llm-content-analysis/pull/3) |
| m-redirection-chain-analysis | `e072c6b` | `dad0e22` | [PR #2](https://github.com/phishark-git/m-redirection-chain-analysis/pull/2) |
| m-favicon-analysis | `8b705d4` | `88fd9b6` | [PR #2](https://github.com/phishark-git/m-favicon-analysis/pull/2) |
| m-content-link-analysis | `a811272` | `7461807` | [PR #2](https://github.com/phishark-git/m-content-link-analysis/pull/2) |
| m-vlm-analysis | `5e40d6a` | `357ebc3` | [PR #2](https://github.com/phishark-git/m-vlm-analysis/pull/2) |

## Verification, separately per repository

Backend, orchestrator, Gatekeeper, domain similarity, LLM content, redirection,
favicon and VLM each passed their own `go test ./...`, `go build ./...` and
`go vet ./...`. Content links has no module file; its documented current CI command
`go test main.go main_test.go` passed, as did `go build ... main.go` and
`go vet main.go main_test.go`. No module layout was introduced merely for testing.

Backend CI passed including Linux race detection at `219c3fd`. Orchestrator CI
passed including race detection at `76ad103` with the full fan-out tests.
Content-link PR CI passed its test/build job.
Provider security-report jobs passed for domain, LLM content, redirection,
favicon, content links and VLM. These report-only jobs are not deployment or
functional/privacy certification. Gatekeeper currently reports no PR checks;
its local tests/build/vet passed.

Browser's shared JS suite passed eight tests including 42 verdict vectors,
IDN/query normalization, stale navigation, private capture rejection,
deduplication/expiry/teardown, total response-read deadline, single capacity retry
and distinct quota/configuration failures. C++ compiled in WSL with warnings as
errors and passed the same 42 vectors plus navigation invariants. Android vault
compiled against the pinned Chromium SDK (android-37.0). Browser contract CI
passed at `51812052` with the HTTP fixture test; platform integration is not
covered by that job.

The eighth JS test uses a real loopback HTTP server and deterministic ephemeral
API/page fixtures. It checks safe/warning/block/prompt and error responses, quota
versus capacity retry, stalled body/cancellation, private deep rejection and
synthetic form/Cookie leaks. Static pages supply redirect, popup, shadow/frame and
scrolled-input fixtures for future native device tests; JS transport success is
not browser-engine acceptance.

Server fixture tests check route authentication, forced non-JSON privacy flags,
quota/accounting without scan creation, preserved existing profile persistence,
suppressed persistence/callback methods, gatekeeper cancellation and transient
cleanup. Full deep fan-out checks prompt safe/suspicious/failure/cancellation and
all expected module results. Provider fixtures preserve returned decisions while
rejecting URL/model/provider-error echoes in logs. All are local fixtures with no
production database or hosted model calls.

## Platform and release gates

Android: Chromium 153.0.8010.37 synced, upstream Cromite patches applied, Linux
dependencies/hooks installed and matching PGO profiles downloaded. ARM64
`chrome_public_apk` and `chrome_public_bundle` built successfully. Initial real Windows
host free space was about 250 GB; it remains monitored during the build. The WSL
virtual disk's reported capacity is not the host SSD capacity. The ARM64 APK
installed but crashed in the emulator's ARM translation JNI path; the official
pinned ARM64 APK failed on the same path. Both the official x64 APK and the local
unchanged x64 baseline launched and opened the fixture page. See artifact hashes and
launch results in [Android emulator evidence](android-emulator-validation.md).

iOS: unchanged Fennec baseline and Swift tests need the user's MacBook, pinned
Xcode 26.5/Swift 6.2 and device/signing verification. No IPA or successful Swift
compilation is claimed from Windows.

Mac handoff: `ios/scripts/mac-verify.sh` records the shared tests, independent
Swift tests and unchanged Fennec simulator build separately. Its baseline helper
uses a fresh, commit/tree-verified standalone upstream checkout so bootstrap's
Git hook installation cannot affect the PhiShark repository. Shell syntax checks
passed in WSL; the helper rejected Linux with exit 2 and saved its preflight
failure report as expected. The eight shared JS tests passed again. These checks
are not evidence of a Mac build; see [iOS instructions](../ios/README.md).

Android NavigationThrottle/JNI, native HTTP/cache, consent, partial HTML capture,
native security dialogs and branding were applied after the local x64 baseline launch. Their
C++ and Java sources passed compatibility compilation against pinned Chromium.
The actual vault source separately passed 16 Android instrumentation assertions
across two processes, including Keystore persistence and authenticated tamper
rejection. Neither result validates the integrated browser. GN generation for
the actual PhiShark package passed; its x64 APK/AAB built, and the APK launched.
After fixing a missing Cromite firewall rule, device fixtures verified preflight
blocking before page GET, post-load deep blocking, return to safety and private
URL-only requests. One normal deep capture passed synthetic sensitive-field/header
checks. Integrated ARM64 build is running; the broader device suite remains incomplete.
Screenshot masking, complete capture, cross-tab request coalescing, final
address-bar integration, Firefox delegate integration, telemetry/account/sync
audit and actual browser-function acceptance remain incomplete. See
[device acceptance](device-acceptance.md).

Live proxy/APM/module retention, active profile flags, internal token setup,
external-provider settings and deployment topology are unknown. Source changes
do not prove live non-retention. [Rollout](cross-repository-rollout.md) separates
implementation order from deployment order and records approval gates.
