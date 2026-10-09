#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
build_root=${1:?Usage: baseline.sh /absolute/Linux/build-root}
architecture=${2:-arm64}
jobs=${3:-16}
[[ "$jobs" =~ ^[1-9][0-9]?$ ]] && (( jobs <= 32 )) || { echo 'Build jobs must be 1–32'; exit 2; }
case "$architecture" in
 arm64) output=out/phishark_baseline ;;
 x64) output=out/phishark_x64_baseline ;;
 *) echo 'Supported baseline architectures: arm64, x64'; exit 2 ;;
esac
[[ "$build_root" = /* && "$build_root" != /mnt/* ]] || { echo 'Use a Linux filesystem build root, not NTFS'; exit 2; }
version=$(tr -d '\r\n' < "$repo_root/android/upstream/build/RELEASE")
mkdir -p "$build_root"
available=$(df -B1 "$build_root" | awk 'NR==2 {print $4}')
(( available >= 180000000000 )) || { echo 'Need 180 GB filesystem free before Chromium sync'; exit 2; }
# WSL virtual disk availability is not host SSD availability. Caller must verify
# Windows host capacity separately before invoking this script.
if [[ ! -d "$build_root/depot_tools/.git" ]]; then
 git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$build_root/depot_tools"
fi
export PATH="$build_root/depot_tools:$PATH"
export DEPOT_TOOLS_UPDATE=0
if [[ ! -f "$build_root/depot_tools/python3_bin_reldir.txt" ]]; then
 DEPOT_TOOLS_DIR="$build_root/depot_tools" bash "$build_root/depot_tools/ensure_bootstrap"
fi
cd "$build_root"
if [[ ! -f .gclient ]]; then
 gclient config --name=src https://chromium.googlesource.com/chromium/src.git
 printf '\ntarget_os = ["android"]\n' >> .gclient
fi
# Cromite's official Android GN configuration enables PGO, including the
# secondary ARM toolchain. Keep upstream optimizations and fetch their profiles.
python3 - "$build_root/.gclient" "$build_root/.chromium-hooks-ready" <<'PY'
from pathlib import Path
import re, sys
config, marker = map(Path, sys.argv[1:])
text = config.read_text()
if not re.search(r"['\"]checkout_pgo_profiles['\"]\s*:\s*True", text):
    text, count = re.subn(r"(['\"]custom_vars['\"]\s*:\s*)\{\s*\}",
                         r"\1{'checkout_pgo_profiles': True}", text, count=1)
    if count != 1:
        raise SystemExit('Review existing .gclient custom_vars before enabling PGO')
    config.write_text(text)
    marker.unlink(missing_ok=True)
PY
if [[ ! -f .chromium-synced ]]; then
 gclient sync --no-history --nohooks --revision "src@$version" -j 8
 printf '%s\n' "$version" > .chromium-synced
fi
[[ $(cat .chromium-synced) = "$version" ]] || { echo 'Use a separate build root for a new Chromium pin'; exit 2; }
cd src
git config user.name 'PhiShark Build'
git config user.email 'build@phishark.io'
if [[ ! -f "$build_root/.cromite-patches-applied" ]]; then
 bash "$repo_root/android/scripts/prepare-dependencies.sh" "$build_root"
 resume_after=$(cat "$build_root/.cromite-patch-progress" 2>/dev/null || true)
 skipping=false; [[ -z "$resume_after" ]] || skipping=true
 while IFS= read -r patch || [[ -n "$patch" ]]; do
  patch=${patch%$'\r'}
  [[ -z "$patch" || "$patch" = \#* ]] && continue
  if $skipping; then
   [[ "$patch" != "$resume_after" ]] || skipping=false
   continue
  fi
  git am "$repo_root/android/upstream/build/patches/$patch"
  printf '%s\n' "$patch" > "$build_root/.cromite-patch-progress"
 done < "$repo_root/android/upstream/build/cromite_patches_list.txt"
 touch "$build_root/.cromite-patches-applied"
fi
if [[ ! -f "$build_root/.build-deps-installed" ]]; then
 sudo -n true || { echo 'Install Chromium build dependencies through WSL root, then mark .build-deps-installed'; exit 2; }
 build/install-build-deps.sh --android --no-prompt
 touch "$build_root/.build-deps-installed"
fi
if [[ ! -f "$build_root/.chromium-hooks-ready" ]]; then
 # gclient enumerates nested repositories even when running only hooks.
 # Temporarily restore preserved metadata without changing patched files.
 bash "$repo_root/android/scripts/prepare-dependencies.sh" "$build_root" restore-metadata
 if [[ ! -f third_party/depot_tools/python3_bin_reldir.txt ]]; then
  DEPOT_TOOLS_DIR="$build_root/src/third_party/depot_tools" bash third_party/depot_tools/ensure_bootstrap
 fi
 gclient runhooks
 bash "$repo_root/android/scripts/prepare-dependencies.sh" "$build_root" hide-metadata
 touch "$build_root/.chromium-hooks-ready"
fi
# The pinned Cromite image's pre-start.sh additionally fetches desktop Android
# PGO profiles; its patches select these for ARM64. Do not replace them with a
# profile from a different Chromium release or silently turn PGO off.
for pgo_target in android-arm32 android-desktop-arm64 android-desktop-x64; do
 if ! python3 tools/update_pgo_profiles.py --target="$pgo_target" get_profile_path >/dev/null 2>&1; then
  python3 tools/update_pgo_profiles.py --target="$pgo_target" update --gs-url-base=chromium-optimization-profiles/pgo_profiles
 fi
done
gn gen "$output" --args="target_os=\"android\" $(cat "$repo_root/android/upstream/build/cromite.gn_args") target_cpu=\"$architecture\" is_debug=false"
autoninja -C "$output" chrome_public_apk chrome_public_bundle -j "$jobs"
