#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) = Darwin ]] || { echo 'Run on the MacBook with Xcode'; exit 2; }
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
xcodebuild -version
swift --version
# This baseline follows the pinned README, not whichever main branch is current.
[[ $(xcodebuild -version | head -1) = 'Xcode 26.5' ]] || { echo 'Pinned Firefox requires Xcode 26.5; review a mismatch before building'; exit 2; }
cd "$repo_root/ios/upstream"
sh ./bootstrap.sh
xcodebuild -project firefox-ios/Client.xcodeproj -scheme Fennec \
 -destination 'generic/platform=iOS Simulator' \
 -derivedDataPath "$repo_root/.build/ios-baseline" CODE_SIGNING_ALLOWED=NO build
cd "$repo_root/ios/security"
swift test
