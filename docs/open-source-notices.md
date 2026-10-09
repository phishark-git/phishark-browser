# Licenses and attribution

The repository is multi-license. Shared PhiShark contract/tests/docs/scripts use Apache-2.0; Android additions use GPL-3.0-only; iOS additions use MPL-2.0. Individual upstream file licenses control those files. This manifest does not relicense imported sources.

Android preserves Cromite's LICENSE and all patch headers. Cromite identifies its aggregate GPLv3 licensing, with Bromite patches GPLv3-only and Cromite patches GPLv2-or-later; Chromium and its third-party code retain their own notices. Ship corresponding source and patch/build material with each release. Chromium sources are external to this Git repository but must remain retrievable at the pinned revision. A build dependency or generated license bundle is not optional attribution.

iOS preserves Mozilla's MPL-2.0 notices, file-level obligations and third-party acknowledgements. Update the native Settings > Licenses bundle when dependencies change. Mozilla/Firefox trademarks and logos are not licensed by MPL. Replace product branding before distribution, including names, icons, bundle IDs, service references and store imagery. The untouched Fennec baseline is for development verification.

Sources: [Cromite license statement](https://github.com/uazo/cromite#license), [Firefox iOS source](https://github.com/mozilla-mobile/firefox-ios/tree/firefox-v157.1), [Mozilla trademark policy](https://www.mozilla.org/en-US/foundation/trademarks/policy/), [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0), [MPL-2.0](https://www.mozilla.org/en-US/MPL/2.0/).

Release gates: generated third-party notices checked against the final APK/AAB and iOS dependency graph; source offer/source archive attached; trademark replacements verified; codec/third-party licensing review completed. These gates are pending, not legal clearance.
