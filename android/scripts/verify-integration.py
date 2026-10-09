#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile overlay sources against a built pinned Chromium tree, without editing it.

This checks native API compatibility, not an APK launch or device acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', default='out/phishark_baseline')
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    out = (source / args.output).resolve(strict=True)
    out.relative_to(source / 'out')
    repo = Path(__file__).resolve().parents[2]
    overlay = repo / 'android/integration/chromium'
    base = repo / '.build'
    base.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='native-overlay-', dir=base) as directory:
        check = Path(directory)
        headers = check / 'gen/chrome/android/chrome_jni_headers'
        headers.mkdir(parents=True)
        bridge = overlay / 'chrome/android/java/src/org/chromium/chrome/browser/phishark/PhiSharkBridge.java'
        subprocess.run(['python3', str(source / 'third_party/jni_zero/jni_zero.py'), 'from-source',
            '--input-file', str(bridge), '--output-dir', str(headers),
            '--shared-header-name', 'PhiSharkBridge_shared_jni.h',
            '--unshared-header-name', 'PhiSharkBridge_jni.h',
            '--allow-private-called-by-natives', '--per-file-natives'], cwd=source, check=True)
        verdict = check / 'gen/chrome/browser/phishark/verdict.h'
        verdict.parent.mkdir(parents=True)
        shutil.copy2(repo / 'android/security/verdict.h', verdict)
        fingerprint = hashlib.sha256(b''.join((out / path).read_bytes() for path in
            ('build.ninja', 'toolchain.ninja', 'obj/chrome/browser/core.ninja'))).hexdigest()
        cache_path = base / ('compiler-command-' + out.name + '.json')
        cached = json.loads(cache_path.read_text()) if cache_path.exists() else {}
        if cached.get('fingerprint') == fingerprint:
            commands = cached['command']
        else:
            commands = subprocess.check_output([str(source / 'third_party/ninja/ninja'), '-C', str(out),
                '-t', 'commands', '-s', 'obj/chrome/browser/core/chrome_content_browser_client_navigation_throttles.o'],
                text=True, errors='replace')
            cache_path.write_text(json.dumps({'fingerprint': fingerprint, 'command': commands}))
        anchor = '../../chrome/browser/chrome_content_browser_client_navigation_throttles.cc'
        lines = [line for line in commands.splitlines() if ' -c ' + anchor in line]
        if len(lines) != 1:
            raise SystemExit(f'Expected one pinned compiler command, found {len(lines)}')
        for fixtures in (False, True):
            (check / 'gen/chrome/browser/phishark_buildflags.h').write_text(
                '#include "build/buildflag.h"\n'
                f'#define BUILDFLAG_INTERNAL_PHISHARK_ALLOW_LOOPBACK_TESTING() ({int(fixtures)})\n')
            arguments = shlex.split(lines[0])
            stem = 'fixtures' if fixtures else 'release'
            for index, value in enumerate(arguments):
                if value == anchor:
                    arguments[index] = str(overlay / 'chrome/browser/phishark/navigation_throttle.cc')
                elif index > 0 and arguments[index - 1] in ('-o', '-MF'):
                    arguments[index] = str(check / (stem + ('.o' if arguments[index - 1] == '-o' else '.d')))
            arguments[1:1] = ['-I' + str(check / 'gen'), '-I' + str(overlay)]
            result = subprocess.run(arguments, cwd=out)
            if result.returncode: raise SystemExit(result.returncode)
            print(f'Chromium native overlay compile passed: {stem}', flush=True)

        sdk = source / 'third_party/android_sdk/public/platforms/android-37.0/android.jar'
        jars = sorted((out / 'obj').rglob('*.turbine.jar'))
        if not jars or not sdk.exists():
            raise SystemExit('The pinned SDK and baseline Java classpath must exist')
        javac = source / 'third_party/jdk/current/bin/javac'
        classes = check / 'classes'; classes.mkdir()
        subprocess.run([str(javac), '-proc:none', '-cp', ':'.join(map(str, [sdk, *jars])),
            '-d', str(classes), str(bridge),
            str(overlay / 'chrome/android/java/src/org/chromium/chrome/browser/phishark/PhiSharkUpdateController.java'),
            str(repo / 'android/security/java/io/phishark/browser/security/ApiKeyVault.java')],
            cwd=source, check=True)
        print('Chromium Java API compatibility compile passed; APK/device tests are separate')


if __name__ == '__main__':
    main()
