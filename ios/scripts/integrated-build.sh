#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "$0")/../.." && pwd)
[[ $(uname -s) = Darwin ]] || { echo 'Run this script on macOS.' >&2; exit 2; }
IFS=$'\t' read -r upstream_url upstream_tag upstream_commit upstream_tree required_xcode <<<"$(
  node -e 'const p=require(process.argv[1]).ios; console.log([p.repository,p.tag,p.commit,p.imported_tree,p.xcode].join("\t"))' \
    "$repo_root/shared/security-contract/upstreams.lock.json"
)"
xcode_version=$(xcodebuild -version)
actual_xcode=${xcode_version%%$'\n'*}
[[ $actual_xcode = "Xcode $required_xcode" ||
   ( $required_xcode = 26.5 && $actual_xcode = 'Xcode 26.6' && ${PHISHARK_XCODE_EXPERIMENT:-} = 26.6 ) ]] || {
  echo "Pinned Firefox requires Xcode $required_xcode; installed $actual_xcode" >&2; exit 2;
}
printf 'Integrated build: pinned Xcode %s; installed %s\n' "$required_xcode" "$actual_xcode"

mkdir -p "$repo_root/.upstream-cache" "$repo_root/.build/ios-integration"
build_root=$(mktemp -d "$repo_root/.upstream-cache/firefox-ios-integrated.XXXXXX")
baseline_project=$(cat "$repo_root/.build/mac-verification/project-path.txt" 2>/dev/null || true)
baseline_root=$(dirname "$(dirname "$baseline_project")")
if [[ -d "$baseline_root/.git" &&
      $(git -C "$baseline_root" rev-parse HEAD) = "$upstream_commit" &&
      $(git -C "$baseline_root" rev-parse 'HEAD^{tree}') = "$upstream_tree" ]]; then
  git clone --local "$baseline_root" "$build_root"
else
  git clone --depth 1 --branch "$upstream_tag" "$upstream_url" "$build_root"
fi
[[ $(git -C "$build_root" rev-parse HEAD) = "$upstream_commit" ]] || {
  echo 'Upstream commit mismatch' >&2; exit 2;
}
[[ $(git -C "$build_root" rev-parse 'HEAD^{tree}') = "$upstream_tree" ]] || {
  echo 'Upstream tree mismatch' >&2; exit 2;
}

# Overlay the deliverable subtree only after verifying the untouched checkout.
rsync -a --delete --exclude=.git "$repo_root/ios/upstream/" "$build_root/"
rm -f "$build_root/firefox-ios/PhiSharkSecurity"
mkdir -p "$build_root/firefox-ios/PhiSharkSecurity"
rsync -a --delete --exclude=.build "$repo_root/ios/security/" "$build_root/firefox-ios/PhiSharkSecurity/"
project_path="$build_root/firefox-ios/Client.xcodeproj"
printf '%s\n' "$project_path" > "$repo_root/.build/ios-integration/project-path.txt"
printf 'Pinned upstream: %s\nIntegrated project: %s\n' "$upstream_commit" "$project_path"
cd "$build_root"
bash -e -o pipefail ./bootstrap.sh
build_args=(-project "$project_path" -scheme Fennec
  -destination "${PHISHARK_DESTINATION:-generic/platform=iOS Simulator}"
  -derivedDataPath "$repo_root/.build/ios-integrated")
if [[ -n ${PHISHARK_BUILD_JOBS:-} ]]; then build_args+=(-jobs "$PHISHARK_BUILD_JOBS"); fi
if [[ -n ${PHISHARK_ARCHS:-} ]]; then build_args+=("ARCHS=$PHISHARK_ARCHS"); fi
xcodebuild "${build_args[@]}" CODE_SIGNING_ALLOWED=NO build
