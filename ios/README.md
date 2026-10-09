# iOS

`upstream/` is the pinned, unmodified Firefox iOS subtree. Its root README specifies Xcode 26.5, Swift 6.2 and iOS 15+. `security/` provides an independent Swift package with decision/session policy, Keychain storage and shared-vector tests. It has not been compiled on Windows.

On the MacBook, install the full Xcode **26.5** application and Node.js **22+**.
Open Xcode once to finish first-launch setup and install the iOS Simulator
platform. Check `xcodebuild -version`; if a different version is installed, report
it before changing the pin. If Command Line Tools are selected instead of Xcode,
select the actual Xcode application under Xcode Settings > Locations > Command
Line Tools. Internet access is needed for upstream and dependency downloads.

Clone this public repository and run the verification helper (no API key or
Apple signing credentials are needed for this simulator build):

```sh
mkdir -p ~/Developer
cd ~/Developer
git clone --depth 1 --single-branch --branch codex/browser-mvp \
  https://github.com/phishark-git/phishark-browser.git
cd phishark-browser
bash ios/scripts/mac-verify.sh
```

For an existing clone, use `git status`, then `git pull --ff-only` on the
`codex/browser-mvp` branch when the worktree is clean; do not overwrite local work.
The helper checks the shared JS tests, independently compiles/tests the Swift
security package, and builds the original Fennec simulator scheme. Swift tests
cover the 42 decision vectors and navigation/private-session policy, not device
Keychain or WebKit integration. Independent checks continue after a test failure
so the report records each result; the overall exit code remains nonzero.

`baseline.sh` creates a fresh standalone shallow checkout in `.upstream-cache/`,
verifies its commit **and source tree** against the lock file, and invokes
upstream bootstrap there. This avoids installing Mozilla's Git hooks into the
PhiShark repository and leaves `ios/upstream/` untouched. It performs only the
Fennec build; `mac-verify.sh` also runs the security tests. Previous checkouts are
retained. Upstream bootstrap fetches the Nimbus helper from application-services
`main`; that external download remains a reproducibility limitation of the
upstream process. SwiftLint is checksum-verified by the pinned upstream script.

After the command finishes, send back:

```sh
cat .build/mac-verification/summary.txt
# If baseline failed, also include the last error lines:
tail -n 60 "$(cat .build/mac-verification/latest-run.txt)/baseline.log"
```

All logs live under the reported `.build/mac-verification/run.*` directory.
This verifies an unchanged Fennec baseline and an independent security package;
the native PhiShark browser integration is still pending. No server rollout or
production scan is part of this test.

After a successful baseline build, open the exact verified project:

```sh
open "$(cat .build/mac-verification/project-path.txt)"
```

Select **Fennec**, choose an installed iPhone simulator and press **Cmd+R**.
Xcode may request trust for upstream's **ModifiedCopy** macro, as documented in
the pinned README; approve that specific upstream macro in Xcode and retry the
build there if the command-line build paused at macro validation. Then verify
launching on a real device with your own signing team. Do not commit signing
credentials or provisioning profiles. Record the Xcode version, simulator/device
and iOS version, launch result and basic browsing/tab/history/bookmark/download
checks; these remain pending until actually run.

After Fennec works, integrate the same security manager into existing action/response policy callbacks in `BrowserViewController+WebViewDelegates.swift`, popup creation, redirects, deep links and session restore. Keep every delegate completion exactly once. Bind the manager to Firefox tab identity and navigation generation; never expose the API key in a script handler. Preserve Firefox's tab/history/bookmark/download/permission/share behavior.

WebKit cannot guarantee stopping every intermediate server redirect before its request is sent. Observe callback/commit destinations, run local redirect fixtures and record platform limitations. Post-load blocking cannot undo JavaScript already executed. Branding, bundle IDs, telemetry/sync removal and native UI are pending baseline/device validation.
