# Pinned native integration points

Inspected source on 2026-10-09; Android hooks are applied in the external-source
checkout after its local x64 baseline launch passed. Integrated device verification
and iOS hooks remain pending. Re-check these interfaces when updating pins.
Native credentials and decisions belong to browser processes, not page scripts.

## Android Chromium 153.0.8010.37 with pinned Cromite patches

The external Linux source `chrome/browser/chrome_content_browser_client.cc`
implements `CreateThrottlesForNavigation(NavigationThrottleRegistry&)` and the
separate `CreateThrottlesForCommitWithoutUrlLoader(...)` registration path.
`NavigationThrottle` is constructed with the registry at this revision, rather
than the older NavigationHandle constructor pattern.

`content/public/browser/navigation_throttle.h` specifies start/redirect/response
callbacks and deferred resume/cancel behavior. `WillRedirectRequest()` sees the
new destination in `navigation_handle()->GetURL()`. Use asynchronous checks with
weak ownership, tab/navigation generation and destruction/cancellation handling;
never destroy WebContents synchronously inside these callbacks.

The no-URL-loader registration enables `WillCommitWithoutUrlLoader()` for
browser-initiated same-document and same-document history navigation, empty
documents and certain other paths. It explicitly excludes renderer-initiated
non-history same-document changes, BFCache and prerender activation. Therefore
one throttle registration is insufficient: observe browser commits/activation
and tab/intent/restore/new-window paths and test the actual interception timing.
Do not promise that a post-commit check prevented a request or prior script.

Preserve original throttle ordering, permission/download semantics and normal
safe-browsing interactions. Only primary main-frame HTTP(S) targets enter this
security pipeline; native/internal security pages need a trusted exclusion.
Deep capture follows a committed page with consent, normalized capture formats,
input masking and the same navigation generation. API key JNI access must stay
outside renderer bridges. PhiShark package/label/icon changes and disabling the
Cromite APK updater are applied; the wider upstream service audit remains pending.

## Firefox iOS firefox-v157.1

Actual delegate source is
`ios/upstream/firefox-ios/Client/Frontend/Browser/BrowserViewController/Extensions/BrowserViewController+WebViewDelegates.swift`.

The action policy is a main-actor method with an escaping decisionHandler. Keep
exactly one completion across existing internal URL, download, custom-scheme,
external-app and new security branches. Response policy is a separate main-actor
**async** method returning `WKNavigationResponsePolicy`; do not graft a second
callback completion onto it.

`createWebViewWith` maps parent/new tabs and calls `addPopupForParentTab`. Its
upstream comment states WebKit performs the request automatically; manually
performing the URLRequest would duplicate or corrupt navigation. Bind the new
tab's security manager before allowing its action path. Redirect observation,
`didCommit`, `didFinish`, failure callbacks, restore and deep-link routing must
all share tab identity/generation and cancellation. Response/redirect callbacks
do not guarantee preventing every intermediate server redirect request.

The pinned source also contains URL-bearing navigation diagnostics, ad telemetry,
Glean uploaders, experiments/Nimbus, Firefox Accounts and sync lifecycle calls.
The PhiShark configuration must audit/disable unnecessary sends and remove
visited-URL diagnostics before privacy acceptance. These features have not been
disabled in the unmodified subtree; no release privacy claim is made yet.

Engine code is excluded from the small repo-local foundation graph, and external
Chromium is not part of the aggregate graph. This source inspection supplies
integration leads, not a refreshed engine graph or platform acceptance result.
Use [local fixtures](local-device-fixtures.md) and
[device acceptance](device-acceptance.md) for observed behavior after integration.
