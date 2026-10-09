#!/usr/bin/env bash
set -euo pipefail
source_root=${1:?Pass pinned Chromium source directory}
repo=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$repo/.build/oauth-tests"
"$source_root/third_party/jdk/current/bin/javac" -d "$repo/.build/oauth-tests" \
  "$repo/android/security/java/io/phishark/browser/security/BrowserOAuth.java" "$repo/android/tests/BrowserOAuthTest.java"
"$source_root/third_party/jdk/current/bin/java" -cp "$repo/.build/oauth-tests" BrowserOAuthTest
python3 "$repo/android/tests/branding_test.py"
