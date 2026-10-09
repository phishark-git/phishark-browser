# Android

The Cromite subtree is unmodified. The external Chromium checkout lives on the WSL Linux filesystem, outside this repository. The security directory contains a tested C++ decision/session core and an Android Keystore vault. The `integration/chromium` overlay contains native navigation, networking, consent and UI code. Its x64 fixture APK runs and passed basic native security smoke checks; **full browser acceptance is incomplete**.

## Baseline

Check the **Windows host** free space, not just WSL `df`, before downloading. The initial check found about 250 GB free. WSL's thin virtual disk reported much more but shares the host SSD. Build root used locally: `~/phishark-browser-build`.

```sh
bash android/scripts/baseline.sh "$HOME/phishark-browser-build"
```

The script uses Chromium 153.0.8010.37 and Cromite's ordered patches and GN arguments. ARM64 targets are `chrome_public_apk` and `chrome_public_bundle`. Downloads, system build dependencies and the build can take substantial time. The documented release-specific Docker image returned a missing manifest during the initial check; using an older image would change the baseline and is not approved.

The local baseline needed bootstrap initialization for both root and nested
depot_tools. Chromium hooks now download PGO profiles; Cromite's pre-start profile
list also supplies Android ARM32 and desktop ARM64/x64 profiles selected by its
patches. The helper keeps the pinned official PGO configuration. Source sync,
patching and build-dependency markers are resumable only in the same build root
and version. Nested Git metadata is preserved by prepare-dependencies.sh before
applying Cromite's dependency patches; it is restored for hooks and hidden again
afterwards. Do not reuse these markers with another release.

To retain diagnostics and the exit status:

```sh
bash android/scripts/run-baseline.sh "$HOME/phishark-browser-build"
```

Its log is `$HOME/phishark-browser-build/baseline.log`. Run one build at a time;
monitor actual Windows host free space throughout. This script produces upstream
baseline targets, not a signed/fully integrated PhiShark release.

For an x86_64 emulator use the `x64` second argument; it selects a separate output
and `baseline-x64.log`. An optional third argument sets jobs (1–32, default 16).
Logs append on resume; completed build actions are retained. The local x64 run
resumed with 32 workers after checking WSL memory and Windows disk capacity.

Native conformance check (Linux/WSL):

```sh
node scripts/native-vectors.mjs
g++ -std=c++17 -Wall -Wextra -Werror -I.build/native android/security/verdict_test.cc android/security/verdict.cc -o .build/native/verdict-test
.build/native/verdict-test
```

## Integration gate

The unmodified ARM64 APK/AAB build passed, but both local and official ARM64 APKs
crash in this x86_64 emulator's JNI translation path. The local native x64 baseline
built and launched the fixture page successfully. The overlay is applied; the
integrated x64 build and basic device checks passed, and ARM64 is building.
See [observed build/launch evidence](../docs/android-emulator-validation.md).
No official prebuilt package is presented as a PhiShark application.

After a locally compiled baseline launch, apply the reviewable overlay:

```sh
python3 android/scripts/apply-integration.py /absolute/Linux/build-root/src --dry-run
python3 android/scripts/apply-integration.py /absolute/Linux/build-root/src --baseline-evidence /absolute/path/to/baseline-report.json
```

The report must identify a local unmodified baseline, contain its exact `apkPath`
and `apkSha256`, and record passed installation and launch. The applier validates
all anchors before writing, preserves original files in build-local backups,
uses a `codex/` source branch and stops on conflicting changes. It does not touch
the imported subtree or produce false launch evidence. Regenerate GN with
`chrome_public_manifest_package="io.phishark.browser"`. Fixture-only builds may
set `phishark_allow_loopback_testing=true` and launch with
`--phishark-local-fixtures`; both are development-only. This mode checks only
synthetic `127.0.0.1:8765` targets, never real URLs with a dummy key or consent.

Preserve the launched baseline APK outside the output directory, then build:

```sh
bash android/scripts/build-integration.sh /absolute/Linux/build-root x64 fixtures 32
```

This reuses baseline objects and replaces its output APK/AAB with development-
signed PhiShark artifacts. Use `release` instead of `fixtures` to compile out the
local fixture mode; this does not supply release signing or certify acceptance.
Do not rerun `baseline.sh` against an integrated source checkout.

The current capture deliberately omits screenshots, frames and custom control
contents. Low-risk partial deep captures stay unverified. Pixel masking, DNS
enforcement, cross-tab request coalescing, complete address-bar layout and native
device acceptance remain required before release.

Read-only source/API compatibility checks for both release and fixture definitions:

```sh
python3 android/scripts/verify-integration.py /absolute/Linux/build-root/src
```

This derives the actual baseline compiler command and generated JNI headers;
it never overwrites a baseline object. Passing it is not an APK/device test.

After the baseline APK has launched: install a browser-process `NavigationThrottle` for primary-main-frame start and redirects, with cancellation tied to WebContents/navigation generation. Bind the Keystore vault through native JNI only. Add observer checks for same-document, BFCache/history, restore, popup/new tab and external intents. The security interstitial must be native and excluded from scanning; blocked results have no override. A deep warning override applies only to the exact navigation. Preserve ordinary Cromite features.

Build branding and `io.phishark.browser` changes, status and consent UI are applied; basic x64 device checks passed, while full acceptance remains pending. Keystore code passed a separate 16-assertion emulator instrumentation test against the actual vault source; this does not verify browser integration or physical hardware backing. C++ tests do not verify Chromium navigation hooks.
