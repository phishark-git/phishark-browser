# App Store readiness

Status: not ready to publish. Firefox iOS uses WebKit; retain WKWebView. Use `io.phishark.browser` as the application bundle identifier and distinct derived identifiers for needed extensions. Replace Mozilla names/icons/marks and disable unnecessary telemetry, account/sync and service traffic after the Fennec baseline passes.

On the MacBook, verify the pinned Xcode/Swift requirements, simulator and real-device builds, signing/provisioning and all extension entitlements. Default-browser entitlement approval is a separate Apple process; do not assume it exists. Review privacy manifests and required-reason APIs for the final dependency graph. Fill App Privacy from actual URL/evidence processing, usage/account metadata and external providers.

Acceptance includes WK action/response callbacks, server/client redirects, popups, restore, history, same-document navigation and external deep links. Document intermediate-redirect limitations; do not advertise pre-request parity with Android. Verify consent, Keychain accessibility/non-sync/removal, private-session teardown, sensitive-field masking, native blocking screens and unchanged normal-browser features.

No TestFlight/App Store submission, signing asset upload or entitlement change has occurred. Simulator Fennec, security Swift tests, physical-device checks and App Privacy review remain pending.

Sources: [Apple review guidelines](https://developer.apple.com/app-store/review/guidelines/), [privacy manifests](https://developer.apple.com/documentation/bundleresources/privacy_manifest_files), [default browser entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.web-browser).
