#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -uo pipefail
build_root=${1:?Usage: run-baseline.sh /Linux/build-root}
architecture=${2:-arm64}
jobs=${3:-16}
case "$architecture" in
 arm64) log="$build_root/baseline.log" ;;
 x64) log="$build_root/baseline-x64.log" ;;
 *) echo 'Supported baseline architectures: arm64, x64'; exit 2 ;;
esac
mkdir -p "$build_root"
bash "$(dirname "$0")/baseline.sh" "$build_root" "$architecture" "$jobs" >> "$log" 2>&1
result=$?
tail -30 "$log"
exit "$result"
