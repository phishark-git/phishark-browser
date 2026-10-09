#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
build_root=${1:?Usage: baseline.sh /absolute/Linux/build-root}
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
cd "$build_root"
if [[ ! -f .gclient ]]; then
 gclient config --name=src https://chromium.googlesource.com/chromium/src.git
 printf '\ntarget_os = ["android"]\n' >> .gclient
fi
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
 gclient runhooks
 bash "$repo_root/android/scripts/prepare-dependencies.sh" "$build_root" hide-metadata
 touch "$build_root/.chromium-hooks-ready"
fi
gn gen out/phishark_baseline --args="target_os=\"android\" $(cat "$repo_root/android/upstream/build/cromite.gn_args") target_cpu=\"arm64\" is_debug=false"
autoninja -C out/phishark_baseline chrome_public_apk chrome_public_bundle -j 16
