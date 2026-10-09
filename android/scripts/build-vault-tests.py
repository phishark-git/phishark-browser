#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a small Android Keystore instrumentation APK using pinned Chromium tools.

This has no launcher or Internet permission. It is not a browser demonstration.
Chromium's public development keystore signs only this local test artifact.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    repo = Path(__file__).resolve().parents[2]
    output = repo / '.build/vault-device-tests'
    output.mkdir(parents=True, exist_ok=True)
    sdk = source / 'third_party/android_sdk/public'
    jar = sdk / 'platforms/android-37.0/android.jar'
    tools = sdk / 'build-tools/37.0.0'
    jdk = source / 'third_party/jdk/current/bin'
    with tempfile.TemporaryDirectory(prefix='compile-', dir=output) as directory:
        work = Path(directory); classes = work / 'classes'; classes.mkdir()
        subprocess.run([str(jdk / 'javac'), '-source', '17', '-target', '17',
            '-cp', str(jar), '-d', str(classes),
            str(repo / 'android/security/java/io/phishark/browser/security/ApiKeyVault.java'),
            str(repo / 'android/tests/vault/VaultInstrumentation.java')], check=True)
        dex = work / 'dex'; dex.mkdir()
        subprocess.run([str(jdk / 'java'), '-cp', str(source / 'third_party/r8/d8/cipd/lib/r8.jar'),
            'com.android.tools.r8.D8', '--lib', str(jar), '--min-api', '29', '--output', str(dex),
            *map(str, classes.rglob('*.class'))], check=True)
        unsigned = work / 'unsigned.apk'
        subprocess.run([str(tools / 'aapt2'), 'link', '-I', str(jar), '--manifest',
            str(repo / 'android/tests/vault/AndroidManifest.xml'), '-o', str(unsigned)], check=True)
        with zipfile.ZipFile(unsigned, 'a') as archive:
            for path in dex.glob('*.dex'): archive.write(path, path.name)
        aligned = work / 'aligned.apk'
        subprocess.run([str(tools / 'zipalign'), '-f', '4', str(unsigned), str(aligned)], check=True)
        apk = output / 'PhiSharkVaultTests.apk'
        subprocess.run([str(jdk / 'java'), '-jar', str(tools / 'lib/apksigner.jar'), 'sign',
            '--ks', str(source / 'build/android/chromium-debug.keystore'),
            '--ks-key-alias', 'chromiumdebugkey', '--ks-pass', 'pass:chromium',
            '--key-pass', 'pass:chromium', '--out', str(apk), str(aligned)], check=True)
        print('Built test-only instrumentation APK: ' + str(apk))


if __name__ == '__main__':
    main()
