# Android build and emulator evidence

Reviewed 2026-10-09. A successful compile is separate from an application launch.
This report does not certify PhiShark Browser or production API protection.

## Unmodified pinned ARM64 build

The pinned Chromium/Cromite source built `chrome_public_apk` and
`chrome_public_bundle` on Ubuntu 24.04 under WSL, with the upstream release GN
arguments, matching pinned PGO profiles and `target_cpu="arm64"`.
The build reported `Build Succeeded: 59981 steps` after 3h7m06s.

| Artifact | SHA-256 |
| --- | --- |
| Local ARM64 ChromePublic.apk | `36cc3434c27d8f154c658f3b20b3304d51d3f26bd260acf0a183b4f38b1a693e` |
| Local ARM64 ChromePublic.aab | `c5c2393039d747b11c6acc73b5832be4352ae00b844496a1ba3a9c9226b47328` |
| Universal APK generated from that AAB | `be3518c315f531c3e66ed5903e967c4fd36d004340427d0e68d5be20e6c007ce` |

Artifacts are kept outside Git in the Linux build workspace, with task-local
Windows copies for installation. They use Chromium's development signing key,
not a PhiShark release key. Package: `org.cromite.cromite`, version:
`153.0.8010.37`. No PhiShark navigation hooks are present in these artifacts.

## Windows emulator comparison

The task-local Android Emulator 37.3.3 runs the official Android 15/API 35 Google
APIs x86_64 r09 image on WHPX. The emulator advertises ARM64 translation.
Only synthetic fixture data is used; the production PhiShark API is not called.

| Artifact | Installation | Observed launch |
| --- | --- | --- |
| Official pinned Cromite x64 APK | Passed | Passed; local fixture page opened |
| Locally compiled unmodified ARM64 APK | Passed; Android reports `arm64-v8a` | Failed: immediate SIGSEGV |
| Locally generated ARM64 universal APK | Passed | Failed: immediate SIGSEGV |
| Official pinned Cromite ARM64 APK | Passed | Failed: same JNI exception path |
| Locally compiled unmodified x64 APK | Passed | Passed; first-run completed and local fixture page rendered |
| PhiShark integration APK | Build running | Pending |

The original ARM64 APK and official ARM64 package both fail when a host JNI
exception crosses the translated guest's `FindClassHook`. Local symbolization
identifies `base::android::FindClassHook` and Android locale-resource lookup.
The universal APK encounters the same hook during a Java filesystem operation.
This comparison supports an ARM translation compatibility issue; it does not
prove that the ARM64 artifacts run correctly on a physical Android device.

The official comparison APKs are diagnostic inputs, never PhiShark releases:

- Official x64 SHA-256: `f63e0c8e97a1796a82d1d2c18ae13115e5a88184e3911b43f903884fe358306a`.
- Official ARM64 SHA-256: `9db12af1af021f42b4371e76d0e9fe476a7085bbbbc76bc410a538d028bf6071`.

The native x64 baseline passed on this Windows emulator. Its build command is:

```bash
bash android/scripts/run-baseline.sh /absolute/Linux/build-root x64
```

It uses a separate `out/phishark_x64_baseline` directory and
`baseline-x64.log`; ARM64 outputs and original sources are preserved. Upstream
launch passed before applying navigation/branding modifications. The final
resumed build segment completed 29,979 actions in 1h39m37s; this excludes earlier
interrupted/resumed work. Local artifacts were copied to an immutable baseline
directory before reusing the output directory for PhiShark:

- x64 APK SHA-256: `42c24b917e36a6739681e8395430a3b93b7666f3f370589c01cafbecdcc36709`.
- x64 AAB SHA-256: `6956baee9dd67979befa0f1d33a4b783e9bd941f001e5a669236c4f756c0545d`.

The observed first-run and local HTTP page render establish baseline launch only.
The 17-file PhiShark overlay was subsequently applied to a `codex/` branch in the
external checkout; GN generation passed for `io.phishark.browser`, with the
synthetic fixture mode enabled. The integrated APK/device acceptance is pending.

## Prepared integration verification

The overlay C++ source has compiled against the actual pinned Chromium ARM64
command, JNI generator and Android headers, with upstream warning/plugin checks.
The Java bridge and vault compiled against the actual Chromium classpath and SDK.
These compiler checks do not run a navigation or prove screenshot privacy.
The overlay was applied after the local x64 baseline launch.
`apply-integration.py --dry-run` validates all 17
copy/edit targets without mutating Chromium.

The actual `ApiKeyVault` source also passed a separate Android instrumentation
test on this emulator: 16 assertions across two different processes, including
encrypted persistence after process restart, absence of plaintext in the stored
file, fresh AES-GCM nonces, tamper rejection, invalid format rejection, key size
limits and deletion of both file and Keystore alias. The test-only package
`io.phishark.browser.vaulttests` has no Internet permission and uses synthetic
data. This verifies the storage component, not the integrated browser, and does
not establish hardware-backed Keystore behavior on a physical device.

Build with `python3 android/scripts/build-vault-tests.py /absolute/Linux/build-root/src`.
Install the resulting `.build/vault-device-tests/PhiSharkVaultTests.apk`, then run
the following commands in order against the test emulator:

```bash
adb -s emulator-5554 shell am instrument -w -e phase prepare io.phishark.browser.vaulttests/io.phishark.browser.vaulttests.VaultInstrumentation
adb -s emulator-5554 shell am instrument -w -e phase verify io.phishark.browser.vaulttests/io.phishark.browser.vaulttests.VaultInstrumentation
```

Native fixture evidence must subsequently establish preflight cancellation
before page GET, redirect rechecks, deep warning/block behavior, private URL-only
operation, capture sanitization, error classification and stale-result rejection.
Fixture `/stats` counts only known synthetic scenario names and numeric events
in memory. It stores no URL or page evidence. An unchanged `preflight-block`
page GET count is the concrete test for preventing that document request.
