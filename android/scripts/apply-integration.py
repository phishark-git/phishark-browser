#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Apply the reviewable Android overlay after a locally built baseline launch.

Dry-run is read-only. The source checkout lives on Linux, outside this repository.
Unrelated source changes are retained; conflicting changes stop the application.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

REPO = Path(__file__).resolve().parents[2]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(text, old, new, label):
    if text.count(old) != 1:
        raise SystemExit(f"Pinned upstream anchor changed: {label}")
    return text.replace(old, new, 1)


def gn_target(text, name, transform):
    anchor = f'{name} {{'
    start = text.index(anchor) + len(anchor)
    depth, quote, escaped, comment = 1, False, False, False
    for end in range(start, len(text)):
        char = text[end]
        if comment:
            if char == '\n': comment = False
            continue
        if quote:
            if escaped: escaped = False
            elif char == '\\': escaped = True
            elif char == '"': quote = False
            continue
        if char == '#': comment = True
        elif char == '"': quote = True
        elif char == '{': depth += 1
        elif char == '}':
            depth -= 1
            if depth == 0:
                return text[:start] + transform(text[start:end]) + text[end:]
    raise SystemExit(f"Unterminated GN target: {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--baseline-evidence', type=Path)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    if not (source / 'chrome/browser/BUILD.gn').is_file() or source == REPO:
        raise SystemExit('Expected the external pinned Chromium source checkout')
    if not args.dry_run:
        if not args.baseline_evidence:
            raise SystemExit('A locally compiled and launched baseline report is required')
        evidence = json.loads(args.baseline_evidence.read_text())
        apk = Path(evidence.get('apkPath', '')).resolve()
        apk.relative_to(source.parent)
        if (evidence.get('artifactKind') not in (
                    'local unmodified Cromite ARM64 baseline', 'local unmodified Cromite x64 baseline')
                or evidence.get('installation') != 'passed'
                or evidence.get('launch') != 'passed'
                or not apk.is_file()
                or digest(apk.read_bytes()) != evidence.get('apkSha256')):
            raise SystemExit('Baseline report must match the actual locally built and launched APK')

    state_root = source.parent / '.phishark-integration'
    manifest_path = state_root / 'manifest.json'
    prior = json.loads(manifest_path.read_text()) if manifest_path.is_file() else None
    changes = {}

    def edit(relative, transform):
        path = source / relative
        current = path.read_bytes()
        if prior and relative in prior['files']:
            if digest(current) != prior['files'][relative]['appliedSha256']:
                raise SystemExit(f'Integration file changed since application: {relative}')
            original = (state_root / 'backups' / relative).read_bytes()
        else:
            dirty = subprocess.check_output(['git', 'status', '--porcelain', '--', relative], cwd=source)
            if dirty:
                raise SystemExit(f'Preserve and review existing changes first: {relative}')
            original = current
        updated = transform(original.decode('utf-8')).encode('utf-8')
        changes[relative] = (current, original, updated)

    java = 'java/src/org/chromium/chrome/browser/phishark/PhiSharkBridge.java'
    updater = 'java/src/org/chromium/chrome/browser/phishark/PhiSharkUpdateController.java'
    vault = 'java/src/io/phishark/browser/security/ApiKeyVault.java'

    def android_gn(text):
        text = replace_once(text, '    sources = chrome_java_sources\n',
            '    sources = chrome_java_sources\n    sources += [\n'
            f'      "{java}",\n      "{updater}",\n      "{vault}",\n'
            '      "java/src/io/phishark/browser/security/BrowserOAuth.java",\n'
            '      "java/src/org/chromium/chrome/browser/phishark/BrowserAccount.java",\n'
            '      "java/src/org/chromium/chrome/browser/phishark/BrowserAccountActivity.java",\n    ]\n', 'chrome_java sources')
        text = gn_target(text, 'generate_jni("chrome_jni_headers")', lambda body:
            replace_once(body, '    sources = [\n',
                f'    allow_private_called_by_natives = true\n    sources = [\n      "{java}",\n', 'JNI sources'))
        return replace_once(text, '      "java/res_base/drawable/ic_launcher.xml",',
            '      "java/res_base/drawable/phishark_icon.xml",\n'
            '      "java/res_base/drawable-nodpi/phishark_mark.png",\n'
            '      "java/res_base/drawable-nodpi/phishark_wordmark.png",\n'
            '      "java/res_base/values/phishark.xml",\n'
            '      "java/res_base/drawable/ic_launcher.xml",', 'icon resources')

    edit('chrome/android/BUILD.gn', android_gn)
    # Only the factory-supplied new-tab suggestions; user history/bookmarks remain
    # controlled by the existing browser model. Do not ship Chromium demo links.
    edit('components/ntp_tiles/resources/default_popular_sites.json', lambda text:
        json.dumps([
            {'title': 'PhiShark', 'url': 'https://phishark.io/'},
            {'title': 'My PhiShark account', 'url': 'https://app.phishark.io/'},
        ], indent=2) + '\n')

    def core(body):
        body = replace_once(body, '  defines = [ "ZLIB_CONST" ]',
            '  defines = [ "ZLIB_CONST" ]\n'
            '  if (is_android) {\n'
            '    sources += [ "phishark/navigation_throttle.cc", "phishark/navigation_throttle.h", "phishark/verdict.cc", "phishark/verdict.h" ]\n'
            '  }', 'core defines')
        return replace_once(body, '  deps = [\n',
            '  deps = [\n    ":phishark_buildflags",\n    "//crypto",\n', 'core dependencies')

    edit('chrome/browser/BUILD.gn', lambda text: gn_target(
        'import("//chrome/browser/phishark/features.gni")\n' + text, 'source_set("core")', core)
        + '\nbuildflag_header("phishark_buildflags") {\n'
        '  header = "phishark_buildflags.h"\n'
        '  flags = [ "PHISHARK_ALLOW_LOOPBACK_TESTING=$phishark_allow_loopback_testing" ]\n}\n')

    def native(text, function_anchor, insertion):
        text = replace_once(text, function_anchor, function_anchor + insertion, 'native hook')
        # Both files already have Android-only include groups after build_config.h.
        return text.replace('#if BUILDFLAG(IS_ANDROID)\n',
            '#if BUILDFLAG(IS_ANDROID)\n#include "chrome/browser/phishark/navigation_throttle.h"\n', 1)

    edit('chrome/browser/chrome_content_browser_client_navigation_throttles.cc', lambda text:
        native(replace_once(text,
            'void CreateAndAddChromeThrottlesForCommitWithoutUrlLoader(\n    content::NavigationThrottleRegistry& registry) {',
            'void CreateAndAddChromeThrottlesForCommitWithoutUrlLoader(\n    content::NavigationThrottleRegistry& registry) {\n'
            '#if BUILDFLAG(IS_ANDROID)\n  phishark::NavigationThrottle::MaybeCreateAndAdd(registry);\n#endif', 'commit throttle'),
            'void CreateAndAddChromeThrottlesForNavigation(\n    content::NavigationThrottleRegistry& registry) {',
            '\n#if BUILDFLAG(IS_ANDROID)\n  phishark::NavigationThrottle::MaybeCreateAndAdd(registry);\n#endif'))
    edit('chrome/browser/ui/tab_helpers.cc', lambda text: native(text,
        'void TabHelpers::AttachTabHelpers(WebContents* web_contents,\n                                  bool enable_browser_autofill) {',
        '\n#if BUILDFLAG(IS_ANDROID)\n  phishark::AttachTabProtection(web_contents);\n#endif'))

    bridge = 'org.chromium.chrome.browser.phishark.PhiSharkBridge'
    edit('chrome/android/java/src/org/chromium/chrome/browser/app/ChromeActivity.java', lambda text:
        replace_once(replace_once(replace_once(text,
            '        if (id == R.id.preferences_id) {',
            f'        if (id == R.id.phishark_protection_id) {{\n            {bridge}.showPanel(this);\n            return true;\n        }}\n\n'
            '        if (id == R.id.preferences_id) {', 'protection menu action'),
            '        super.finishNativeInitialization();',
            f'        super.finishNativeInitialization();\n        {bridge}.install(this, getActivityTabProvider());', 'activity install'),
            '    protected void onDestroyInternal() {',
            f'    protected void onDestroyInternal() {{\n        {bridge}.uninstall(this);', 'activity teardown'))
    def protection_menu(text):
        anchor = '        modelList.add(buildSettingsItem());'
        if text.count(anchor) != 4:
            raise SystemExit('Pinned upstream menu variants changed')
        text = text.replace(anchor, '        modelList.add(buildPhiSharkItem());\n' + anchor)
        return replace_once(text, '    private ListItem buildSettingsItem() {',
            '    private ListItem buildPhiSharkItem() {\n'
            '        return new ListItem(AppMenuHandler.AppMenuItemType.STANDARD,\n'
            '                AppMenuItemUtils.buildModelForStandardMenuItem(\n'
            '                        mContext, getAppMenuItemTheme(), R.id.phishark_protection_id,\n'
            '                        R.string.phishark_protection_menu,\n'
            '                        shouldShowIconBeforeItem() ? R.drawable.phishark_icon : Resources.ID_NULL,\n'
            '                        isMenuIconAtStart()));\n    }\n\n'
            '    private ListItem buildSettingsItem() {', 'protection menu model')
    edit('chrome/android/java/src/org/chromium/chrome/browser/tabbed_mode/TabbedAppMenuPropertiesDelegate.java', protection_menu)
    edit('chrome/android/java/AndroidManifest.xml', lambda text:
        replace_once(replace_once(replace_once(text,
            'android:label="Cromite"', 'android:label="PhiShark Browser"', 'application label'),
            '        {% block extra_application_definitions %}',
            '<activity android:name="org.chromium.chrome.browser.phishark.BrowserAccountActivity" '
            'android:exported="true" android:excludeFromRecents="true">\n'
            '  <intent-filter><action android:name="android.intent.action.VIEW"/>\n'
            '    <category android:name="android.intent.category.DEFAULT"/>\n'
            '    <category android:name="android.intent.category.BROWSABLE"/>\n'
            '    <data android:scheme="io.phishark.browser"/>\n'
            '  </intent-filter>\n</activity>\n        {% block extra_application_definitions %}', 'native account callback'),
            '      <queries>', '      <queries>\n'
            '        <intent><action android:name="android.intent.action.VIEW"/>'
            '<category android:name="android.intent.category.BROWSABLE"/>'
            '<data android:scheme="https"/></intent>', 'external browser visibility'))
    edit('tools/grit/grit/grd_reader.py', lambda text:
        replace_once(text, '    return content\n',
            '    from grit.phishark_brand import brand_message\n'
            '    message_name = next((node.attrs.get("name", "") for node in reversed(self.stack) if node.name == "message"), "")\n'
            '    return brand_message(content_orig, content, message_name)\n', 'product text branding'))
    # Cromite denies browser-process traffic unless its exact annotation has an
    # entry and an allow rule. Keep default-deny and all other rules unchanged.
    annotation = 'phishark_ephemeral_browser_analysis'
    annotation_hash = 0
    for char in annotation:
        annotation_hash = (annotation_hash * 31 + ord(char)) % 138003713
    def firewall_annotations(text):
        if re.search(rf'hash_code="{annotation_hash}"', text):
            raise SystemExit('PhiShark traffic annotation hash collides with an upstream entry')
        return replace_once(text, '</annotations>',
            f' <item id="{annotation}" hash_code="{annotation_hash}" '
            'file_path="chrome/browser/phishark/navigation_throttle.cc" />\n</annotations>', 'firewall annotation')
    edit('services/firewall/tools/annotations.xml', firewall_annotations)
    edit('services/firewall/tools/rules.xml', lambda text:
        replace_once(text, '</rules>',
            f' <!-- User-configured ephemeral PhiShark URL/content analysis only. -->\n'
            f' <item id="{annotation}" allowed="1"/>\n</rules>', 'firewall rule'))

    def first_run(text):
        text = replace_once(text, '        mTitle = view.findViewById(R.id.title);',
            '        mTitle = view.findViewById(R.id.title);\n'
            '        ((TextView) mTitle).setText("PhiShark Browser");\n'
            '        ((android.widget.ImageView) view.findViewById(R.id.image))\n'
            '                .setImageResource(R.drawable.phishark_icon);', 'welcome branding')
        text = replace_once(text, '        mAutoUpdaterCheckBox.setVisibility(visibility);',
            '        mAutoUpdaterCheckBox.setVisibility(View.GONE);', 'disabled upstream updater control')
        text = replace_once(text, '    private void updateReportCheckbox() {',
            '    private void updateReportCheckbox() {\n'
            '        mAutoUpdaterCheckBox.setVisibility(View.GONE);', 'initial updater visibility')
        for number in (3, 4):
            text = replace_once(text, f'        spans.add(buildPrivacyPolicyLink("{number}", R.string.privacy_link{number}));',
                '', 'unused updater privacy link')
        for number, name in ((1, 'terms'), (2, 'privacy')):
            text = replace_once(text, f'buildPrivacyPolicyLink("{number}", R.string.privacy_link{number})',
                f'buildPrivacyPolicyLink("{number}", R.string.phishark_{name}_url)', 'PhiShark legal link')
        return replace_once(text, '        String tosString = getString(R.string.bromite_fre_footer_privacy_policy);',
            '        String tosString = getString(R.string.phishark_welcome_body);',
            'welcome privacy text')
    edit('chrome/android/java/src/org/chromium/chrome/browser/firstrun/ToSAndUMAFirstRunFragment.java', first_run)
    edit('chrome/android/java/src/org/chromium/chrome/browser/omaha/CromiteUpdateStatusProvider.java',
        lambda text: replace_once(text, 'super(new BromiteInlineUpdateController());',
            'super(new org.chromium.chrome.browser.phishark.PhiSharkUpdateController());', 'upstream APK updater'))
    for icon in ('ic_launcher.xml', 'ic_launcher_round.xml'):
        edit('chrome/android/java/res_base/drawable/' + icon, lambda text:
            re.sub(r'(<monochrome android:drawable=")[^"]+', r'\1@drawable/phishark_mark',
                re.sub(r'(<foreground android:drawable=")[^"]+', r'\1@drawable/phishark_icon', text)))

    overlay = REPO / 'android/integration/chromium'
    for path in sorted(overlay.rglob('*')):
        if not path.is_file() or '__pycache__' in path.parts or path.suffix == '.pyc': continue
        relative = path.relative_to(overlay).as_posix()
        target = source / relative
        current = target.read_bytes() if target.exists() else None
        if current is not None and (not prior or relative not in prior['files']
                or digest(current) != prior['files'][relative]['appliedSha256']):
            raise SystemExit(f'Overlay target already contains other content: {relative}')
        changes[relative] = (current, None, path.read_bytes())
    shared_sources = {
        'chrome/android/' + vault: 'android/security/java/io/phishark/browser/security/ApiKeyVault.java',
        'chrome/android/java/src/io/phishark/browser/security/BrowserOAuth.java': 'android/security/java/io/phishark/browser/security/BrowserOAuth.java',
        'chrome/browser/phishark/verdict.h': 'android/security/verdict.h',
        'chrome/browser/phishark/verdict.cc': 'android/security/verdict.cc',
    }
    for relative, repo_relative in shared_sources.items():
        target = source / relative
        current = target.read_bytes() if target.exists() else None
        if current is not None and (not prior or relative not in prior['files']
                or digest(current) != prior['files'][relative]['appliedSha256']):
            raise SystemExit(f'Shared source target already contains other content: {relative}')
        changes[relative] = (current, None, (REPO / repo_relative).read_bytes())

    # All paths/anchors are validated before the first mutation.
    for relative in changes:
        (source / relative).resolve().relative_to(source)
    print(f'{len(changes)} integration files validated; dry_run={args.dry_run}')
    if args.dry_run: return
    branch = subprocess.check_output(['git', 'branch', '--show-current'], cwd=source, text=True).strip()
    if not branch.startswith('codex/'):
        subprocess.run(['git', 'switch', '-c', 'codex/phishark-browser-integration'], cwd=source, check=True)
    records = {}
    for relative, (current, original, updated) in changes.items():
        if original is not None:
            backup = state_root / 'backups' / relative
            if not backup.exists():
                backup.parent.mkdir(parents=True, exist_ok=True); backup.write_bytes(original)
        target = source / relative
        if current != updated:
            target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(updated)
        records[relative] = {'appliedSha256': digest(updated), 'originalSha256': digest(original) if original is not None else None}
    state_root.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps({'files': records}, indent=2) + '\n')
    print('Applied native Android integration; regenerate GN with package io.phishark.browser')


if __name__ == '__main__':
    main()
