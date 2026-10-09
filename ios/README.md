# iOS

`upstream/` is the pinned, unmodified Firefox iOS subtree. Its root README specifies Xcode 26.5, Swift 6.2 and iOS 15+. `security/` provides an independent Swift package with decision/session policy, Keychain storage and shared-vector tests. It has not been compiled on Windows.

On the MacBook, clone this repository's `codex/browser-mvp` branch and run:

```sh
bash ios/scripts/baseline.sh
```

The script builds the original Fennec simulator scheme and runs the security package tests. Then open `ios/upstream/firefox-ios/Client.xcodeproj`, select Fennec and verify launching on a real device with your own signing team. Do not commit signing credentials or provisioning profiles. Xcode may request trust for upstream's ModifiedCopy macro, as documented in the pinned README.

After Fennec works, integrate the same security manager into existing action/response policy callbacks in `BrowserViewController+WebViewDelegates.swift`, popup creation, redirects, deep links and session restore. Keep every delegate completion exactly once. Bind the manager to Firefox tab identity and navigation generation; never expose the API key in a script handler. Preserve Firefox's tab/history/bookmark/download/permission/share behavior.

WebKit cannot guarantee stopping every intermediate server redirect before its request is sent. Observe callback/commit destinations, run local redirect fixtures and record platform limitations. Post-load blocking cannot undo JavaScript already executed. Branding, bundle IDs, telemetry/sync removal and native UI are pending baseline/device validation.
