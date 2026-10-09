#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
build_root=${1:?Usage: build-integration.sh /absolute/Linux/build-root arm64|x64 release|fixtures [jobs]}
architecture=${2:?Specify arm64 or x64}
mode=${3:?Specify release or fixtures}
jobs=${4:-16}
[[ "$build_root" = /* && "$build_root" != /mnt/* ]] || { echo 'Use the existing Linux build root'; exit 2; }
[[ "$jobs" =~ ^[1-9][0-9]?$ ]] && (( jobs <= 32 )) || { echo 'Build jobs must be 1–32'; exit 2; }
case "$architecture" in
 arm64) output=out/phishark_baseline ;;
 x64) output=out/phishark_x64_baseline ;;
 *) echo 'Supported architectures: arm64, x64'; exit 2 ;;
esac
case "$mode" in
 release) fixtures=false ;;
 fixtures) fixtures=true ;;
 *) echo 'Supported modes: release, fixtures'; exit 2 ;;
esac
[[ -f "$build_root/.phishark-integration/manifest.json" ]] || {
 echo 'Apply the integration with passed local baseline evidence first'; exit 2;
}
# Reuse compiled objects, but preserve the launched upstream APK separately
# before replacing output artifacts. The applier validates that evidence.
export PATH="$build_root/depot_tools:$PATH"
export DEPOT_TOOLS_UPDATE=0
cd "$build_root/src"
gn gen "$output" --args="target_os=\"android\" $(cat "$repo_root/android/upstream/build/cromite.gn_args") target_cpu=\"$architecture\" is_debug=false chrome_public_manifest_package=\"io.phishark.browser\" phishark_allow_loopback_testing=$fixtures"
autoninja -C "$output" chrome_public_apk chrome_public_bundle -j "$jobs" 2>&1 | tee -a "$build_root/phishark-$architecture-$mode.log"
echo "Built PhiShark $architecture/$mode development-signed artifacts in $output/apks"
echo 'Build completion does not establish device acceptance or release readiness.'
