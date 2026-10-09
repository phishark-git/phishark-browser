# Upstream update strategy

Pins live in `shared/security-contract/upstreams.lock.json`. Subtree history records the original upstream commits; no Chromium source is copied into this repository. `npm run check:upstream` checks GitHub's latest releases and returns exit 2 when the latest tag differs. It never merges or updates pins automatically. A failed network check is not proof that a version is current.

For each update: inspect security/release notes; fetch the candidate tag; build the unchanged baseline on WSL/Mac; review upstream navigation, permissions, telemetry, updater and license changes; merge the subtree on a `codex/` branch; rebase minimal platform changes; run shared/native/engine/device/privacy and normal-browser tests; record exact hashes and toolchains. Security patches must not be silently skipped just to keep local patches applying.

Android baseline follows Cromite's ordered patch list and GN arguments. Compare Chromium NavigationThrottle/observer APIs at the pinned revision, including same-document and BFCache/prerender behavior. Keep release APK/AAB and baseline artifacts distinct. Do not use an old Docker image to masquerade as the pinned release.

iOS baseline is Fennec. Verify the upstream README's toolchain before bootstrapping dependencies. Compare existing delegate and popup/restore flows; retain single-completion behavior. Re-audit licenses/marks and provider terms at every release.

Each update needs a source archive/notice bundle and rollback to the previous tested mobile release. API compatibility migration precedes client publication. No automatic production deployment or store publication is authorized by an upstream update.
