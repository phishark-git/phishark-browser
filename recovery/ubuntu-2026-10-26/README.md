# Ubuntu browser source recovery — 2026-10-26

Backup taken before deleting reproducible build outputs and selected caches.
The complete Ubuntu source checkout and its Git history remain in place.
The parent commit of this backup branch preserves the main PhiShark browser
project, its pinned Cromite patch set, dependency/build scripts, tests and licenses.

## Contents and verification

- source-overlay: exact copies of 17 modified files and 19 new source files.
- source-manifest.json: per-file SHA-256, size, and tracked/untracked status.
- working-tree.patch: full-index binary Git diff of the 17 tracked files.
- build-config: .gclient, dependency pins and both original output args.gn files.
- integration-state: application manifest and original integration file backups.
- baseline-commits.txt: original local baseline commit ancestry.
- removed-build-artifacts.json: names and sizes of replaceable APK/AAB outputs.

Chromium upstream base: b75a5a95ea1a1b55bdbfd6d9f42d47be7507fb8b
Local patched baseline: 2e10f8eeeeb270f60ce13a310dfe08499aa3a05a

## Restore

For the preserved Ubuntu checkout, source-overlay files can be verified against
source-manifest.json or restored to matching relative paths. Preserve any newer
local work before applying this historical snapshot.

For a fresh machine, use the parent repository's android/scripts/baseline.sh and
prepare-dependencies.sh, its pinned android/upstream/build/RELEASE and complete
cromite_patches_list.txt, and the saved .gclient/dependency pins to recreate the
Chromium/Cromite source and flattened dependencies. Do not treat this directory
as a standalone Chromium checkout. Overlay the 36 source-overlay files on that
baseline and verify their hashes. working-tree.patch provides the tracked-file
delta as an alternative; git apply --check must pass before applying it.

Restore build-config/out/*/args.gn into corresponding output folders and rebuild
with the repository's Android build scripts. The x64 args enable loopback testing;
these are development settings, not a production release configuration. A fresh
native rebuild is required after cleanup and can take significant time/space.

No signing keys, credentials, APK/AAB packages, compiled objects or downloaded
dependencies are uploaded. Upstream Chromium/Cromite and PhiShark license notices
continue to apply. This backup does not deploy providers or publish a release.
