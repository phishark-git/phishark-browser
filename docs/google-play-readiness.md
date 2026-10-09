# Google Play readiness

Status: not ready to publish. Required app identity: `io.phishark.browser`, PhiShark Browser. Keep signing keys outside Git and use a release AAB. Verify the target API requirement again on submission; the plan's minimum is API 36 unless the current policy requires higher.

Preserve daily-browser features and review runtime permissions for downloads, uploads, camera/microphone, geolocation, notifications and file access. Use the platform's appropriate scoped access rather than requesting unnecessary storage privileges. Verify default-browser intent filters, exported components, deep-link input validation and private-mode external intents.

Audit Cromite update/telemetry/account/sync services. The Play flavor must not self-update executable code through Cromite's upstream APK updater. Remove Mozilla/Chromium/Cromite product branding where distribution requires replacement, without removing notices. Produce a privacy policy and accurate Data Safety disclosures for URLs/page evidence, API identifiers, usage counters and external processing. Never claim zero collection merely because browser scans skip history.

Required evidence: installed ARM64 release build; API 36/current target; signed AAB validation; native security/redirect/private-mode fixtures; crash-free browsing, history/bookmarks/downloads/permissions/sharing; masked capture; retention/log audit; accessibility/localization; third-party notices; signing/update recovery. Store publication requires separate approval.

Sources: [target API requirements](https://support.google.com/googleplay/android-developer/answer/11926878), [Data Safety](https://support.google.com/googleplay/android-developer/answer/10787469).
