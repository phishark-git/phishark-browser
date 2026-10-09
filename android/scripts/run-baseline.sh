#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -uo pipefail
build_root=${1:?Usage: run-baseline.sh /Linux/build-root}
mkdir -p "$build_root"
bash "$(dirname "$0")/baseline.sh" "$build_root" > "$build_root/baseline.log" 2>&1
result=$?
tail -30 "$build_root/baseline.log"
exit "$result"
