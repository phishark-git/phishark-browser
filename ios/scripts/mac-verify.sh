#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
report_root="$repo_root/.build/mac-verification"
mkdir -p "$report_root"
log_dir=$(mktemp -d "$report_root/run.XXXXXX")
summary="$log_dir/summary.txt"
printf '%s\n' "$log_dir" > "$report_root/latest-run.txt"
printf 'PhiShark Mac baseline verification\nUTC: %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" > "$summary"
finish() {
  local result=$?
  printf 'Overall exit: %s\n' "$result" >> "$summary"
  cp "$summary" "$report_root/summary.txt"
  cat "$summary"
  printf '\nLogs: %s\n' "$log_dir"
}
trap finish EXIT
fail() { printf 'Preflight: FAIL - %s\n' "$1" >> "$summary"; echo "$1" >&2; exit 2; }
[[ $(uname -s) = Darwin ]] || fail 'Run this script on the MacBook.'
for tool in git node npm xcodebuild swift; do
  command -v "$tool" >/dev/null 2>&1 || fail "Missing tool: $tool"
done
node -e 'if (Number(process.versions.node.split(".")[0]) < 22) process.exit(1)' || fail 'Node.js 22+ is required.'
required_xcode=$(node -e 'console.log(require(process.argv[1]).ios.xcode)' "$repo_root/shared/security-contract/upstreams.lock.json")
xcode_version=$(xcodebuild -version) || fail 'Select the full Xcode application, open it once and finish setup.'
actual_xcode=${xcode_version%%$'\n'*}
if [[ $actual_xcode != "Xcode $required_xcode" ]]; then
  [[ $required_xcode = 26.5 && $actual_xcode = 'Xcode 26.6' && ${PHISHARK_XCODE_EXPERIMENT:-} = 26.6 ]] ||
    fail "This pin requires Xcode $required_xcode. Report your version before changing the pin."
  printf 'Xcode compatibility experiment: pinned %s; installed 26.6 (unverified)\n' "$required_xcode" >> "$summary"
fi
{
  printf 'Browser commit: %s\n' "$(git -C "$repo_root" rev-parse HEAD)"
  printf 'Architecture: %s\nmacOS: %s\n' "$(uname -m)" "$(sw_vers -productVersion)"
  printf '%s\n' "$xcode_version"
  printf 'Pinned Xcode: %s\nInstalled Xcode: %s\n' "$required_xcode" "$actual_xcode"
  swift --version
  printf 'Node: %s\n' "$(node --version)"
  printf 'Build destination: %s\nBuild jobs: %s\nBuild architectures: %s\n' \
    "${PHISHARK_DESTINATION:-generic/platform=iOS Simulator}" \
    "${PHISHARK_BUILD_JOBS:-Xcode default}" "${PHISHARK_ARCHS:-Xcode default}"
} >> "$summary"
printf 'Preflight: PASS\n' >> "$summary"
failed=0
run_check() {
  local label=$1
  shift
  if "$@" 2>&1 | tee "$log_dir/$label.log"; then
    printf '%s: PASS\n' "$label" >> "$summary"
  else
    local result=$?
    printf '%s: FAIL (exit %s)\n' "$label" "$result" >> "$summary"
    failed=1
  fi
}
cd "$repo_root"
run_check shared npm test
run_check swift swift test --package-path "$repo_root/ios/security"
run_check baseline bash "$repo_root/ios/scripts/baseline.sh"
printf 'Device launch: NOT TESTED\nNative PhiShark integration: PENDING\n' >> "$summary"
if [[ -f "$report_root/project-path.txt" ]]; then
  printf '\nTo open the last verified Fennec project:\nopen "$(cat .build/mac-verification/project-path.txt)"\n'
fi
exit "$failed"
