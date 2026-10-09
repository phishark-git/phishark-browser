# Android

The Cromite subtree is unmodified. The external Chromium checkout lives on the WSL Linux filesystem, outside this repository. The security directory contains a tested C++ decision/session core and an Android Keystore vault; these are **not yet wired into a runnable browser**.

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

Native conformance check (Linux/WSL):

```sh
node scripts/native-vectors.mjs
g++ -std=c++17 -Wall -Wextra -Werror -I.build/native android/security/verdict_test.cc -o .build/native/verdict-test
.build/native/verdict-test
```

## Integration gate

After the baseline APK has launched: install a browser-process `NavigationThrottle` for primary-main-frame start and redirects, with cancellation tied to WebContents/navigation generation. Bind the Keystore vault through native JNI only. Add observer checks for same-document, BFCache/history, restore, popup/new tab and external intents. The security interstitial must be native and excluded from scanning; blocked results have no override. A deep warning override applies only to the exact navigation. Preserve ordinary Cromite features.

Build branding and `io.phishark.browser` changes, address-bar status and consent UI remain pending until the baseline passes. Keystore code compiled against the pinned Chromium SDK (android-37.0); device tests remain pending. C++ tests do not verify vault behavior or Chromium navigation hooks.
