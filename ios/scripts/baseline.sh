#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) = Darwin ]] || { echo 'Run on the MacBook with Xcode'; exit 2; }
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
IFS=$'\t' read -r upstream_url upstream_tag upstream_commit upstream_tree required_xcode <<<"$(
  node -e 'const p=require(process.argv[1]).ios; console.log([p.repository,p.tag,p.commit,p.imported_tree,p.xcode].join("\t"))' \
    "$repo_root/shared/security-contract/upstreams.lock.json"
)"
xcode_version=$(xcodebuild -version)
printf '%s\n' "$xcode_version"
[[ ${xcode_version%%$'\n'*} = "Xcode $required_xcode" ]] || {
  echo "Pinned Firefox requires Xcode $required_xcode; review a mismatch before building"; exit 2;
}
swift --version

# Upstream bootstrap installs .git/hooks and expects a standalone Git root.
# Keep its tooling and generated files out of the imported subtree. Every run
# uses a fresh checkout; no previous checkout or user's files are reset/deleted.
mkdir -p "$repo_root/.upstream-cache" "$repo_root/.build/mac-verification"
baseline_root=$(mktemp -d "$repo_root/.upstream-cache/firefox-ios.XXXXXX")
git clone --depth 1 --branch "$upstream_tag" "$upstream_url" "$baseline_root"
[[ $(git -C "$baseline_root" rev-parse HEAD) = "$upstream_commit" ]] || {
  echo 'Upstream tag no longer matches the locked commit; refusing to build'; exit 2;
}
[[ $(git -C "$baseline_root" rev-parse 'HEAD^{tree}') = "$upstream_tree" ]] || {
  echo 'Upstream source tree does not match the imported subtree; refusing to build'; exit 2;
}
project_path="$baseline_root/firefox-ios/Client.xcodeproj"
printf '%s\n' "$project_path" > "$repo_root/.build/mac-verification/project-path.txt"
printf 'Verified upstream: %s\nProject: %s\n' "$upstream_commit" "$project_path"
cd "$baseline_root"
# Surface failed dependency downloads rather than continuing to xcodebuild.
bash -e -o pipefail ./bootstrap.sh
xcodebuild -project firefox-ios/Client.xcodeproj -scheme Fennec \
 -destination 'generic/platform=iOS Simulator' \
 -derivedDataPath "$repo_root/.build/ios-baseline" CODE_SIGNING_ALLOWED=NO build
