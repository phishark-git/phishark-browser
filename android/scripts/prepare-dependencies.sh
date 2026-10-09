#!/usr/bin/env bash
set -euo pipefail
build_root=$(realpath "${1:?Usage: prepare-dependencies.sh /Linux/build-root}")
mode=${2:-flatten}
[[ "$mode" = flatten || "$mode" = restore-metadata || "$mode" = hide-metadata ]] || exit 2
source_root=$(realpath "$build_root/src")
[[ "$source_root" = "$build_root/src" && "$source_root" != /mnt/* ]] || exit 2
[[ -d "$source_root/.git" ]] || { echo 'Expected isolated Chromium checkout'; exit 2; }
cd "$source_root"
mkdir -p "$build_root/.upstream-git-backup"
# These are precisely the repositories flattened by Cromite's pinned
# tools/images/cromite-source/apply-cromite-patches.sh. Keep their metadata
# instead of deleting it; change only the isolated build checkout's index.
for path in v8 third_party/devtools-frontend/src third_party/skia third_party/perfetto third_party/boringssl/src; do
 resolved=$(realpath "$path")
 [[ "$resolved" = "$source_root/$path" ]] || { echo 'Unexpected dependency path'; exit 2; }
 backup="$build_root/.upstream-git-backup/${path//\//_}"
 if [[ "$mode" = restore-metadata ]]; then
  if [[ -e "$backup" ]]; then
   [[ ! -e "$path/.git" ]] || exit 2
   mv "$backup" "$path/.git"
  fi
  continue
 fi
 if [[ -e "$path/.git" ]]; then
  [[ ! -e "$backup" ]] || { echo 'Metadata backup already exists; inspect before overwriting'; exit 2; }
  mv "$path/.git" "$backup"
 fi
 [[ "$mode" = flatten ]] || continue
 if git ls-files --stage "$path" | grep -q '^160000 '; then git rm --cached -f "$path"; fi
 git add -f "$path"
 if ! git diff --cached --quiet; then git commit --quiet -m ":NOEXPORT: $path repo"; fi
done
