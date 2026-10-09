# Cross-repository impact check

## Blocked-result inspection — 2026-10-10

Browser-only UI follow-up queried `ScreenshotCapture PublishRequestCounts showVerdict`
before editing (127 connected nodes). Native risk dialogs now open the existing
protection panel; API/result, credentials, capture, navigation block and provider
contracts are unchanged. No server rollout is required. The x64 APK/AAB and local
tests passed, and a live preflight-block panel confirmed score 100, URL POSTs 1,
deep POSTs 0, cache/retries 0. The requested URL is not retained in repo evidence.
Browser graph refreshed: 672 nodes / 1,391 raw edges. Its component and workspace
were rebuilt without reselecting unaffected branch snapshots; workspace 13,940
nodes / 30,046 edges, no aggregate dangling endpoints. Repeated query found 128
connected nodes (10 displayed at 550 tokens). Semantic-document, external engine
and mixed-branch limitations below still apply; no VDS access or deployment.

## Mobile HTML / PNG evidence — 2026-10-10

Original query `buildWebSubmoduleRequestBody screenshot captureWebEvidence`
was verified against current browser, backend, orchestrator and analyzer source.
The initial graph returned 70 connected nodes (31 displayed at 1,400 tokens).
Providers already accept sanitized HTML/base64 PNG. The changed orchestrator
forces scoped, non-mutating module projections for ephemeral deep only; Android
adds native visible-surface capture/redaction. Existing extension/Playwright
global-flag behavior, mobile public schemas, quotas and callback/persistence
contracts are preserved. Backend/LLM/VLM require no source rollout for this change.
Implementation order: orchestrator projection, Android capture, independent
validation, Mac handoff. Deployment order: reviewed orchestrator workflow after
explicit approval, then accepted clients. PR #25 CI passed; deployment remains
pending and production configuration/provider retention remains unknown.

Both changed repository code graphs were updated. Browser: 672 nodes / 1,390 raw
edges, component 1,198 edges after 180 unresolved external endpoints were dropped.
Orchestrator: 1,721 nodes / 4,622 raw edges, component 3,753 edges after 860
unresolved external endpoints were dropped. Final workspace refresh rebuilt to
13,940 nodes / 30,045 edges, zero aggregate dangling endpoints. The same impact query returned
122 connected nodes (30 displayed at 1,400 tokens), including ScreenshotCapture,
TabProtection, executor builders and selectWebEvidenceResponse.

A concurrent aggregate refresh temporarily selected the separate
`rendered-page-classification` orchestrator worktree. Only this task's browser
and orchestrator components were reselected/refreshed, preserving unrelated
component snapshots, and the query again resolved executor methods to
`browser-orchestrator-20261009` (60b2ceb). Aggregate branch provenance can change
with concurrent workspace work; current source and per-repository tests remain
the acceptance evidence. No semantic completeness is claimed.

Coverage remains incomplete: code-only updates omit document semantics, the
orchestrator's 12 old semantic hyperedges were not preserved in its code-only
component, four browser concepts lack source_file, and unaffected components
retain mixed branch snapshots and old node IDs. The external Chromium checkout
is not the overlay graph. Zero dangling endpoints does not prove complete
coverage. Mac snapshot implementation/device tests and native PNG pixel acceptance
are reported separately; no unresolved compatible consumer is silently
presented as implemented. No VDS access, production operation or workflow change
was performed for this evidence projection.

Reviewed 2026-10-09. Initial graph queries preceded implementation. Paths were
checked against current source, repository AGENTS, SYSTEM_MAP and the LLM
integration inventory; inferred/ambiguous graph edges were treated as leads.

Original query, repeated after refresh:

```text
CreateProfileScan ExecuteProfileScan web_evidence PersistScanComplete callback preflight browser-deep
```

Additional privacy/configuration queries included:

```text
CheckTargetOffline checkOffline performDomainCheck Analyze AnalysisRequest target logging domain similarity
handleAnalyze analyzeURLDirect checkOffline logGatekeeperMatch content analysis handlers
AnalyzeHandler applyThreatIntelIfUnknown SelectFaviconWithLLM callGeminiAPI Batch check HandleAnalyze
CreateBrowserScan authorizeBrowserScan browser_scan_not_configured
```

## Verified dependency findings

Browser → public backend → orchestrator → offline Gatekeeper/domain-only preflight
and caller-evidence deep analyzers → decision-maker/prompt policy. Domain fetcher
results also trigger their DNS/SSL/WHOIS analyzers. Browser deep has no internal
web fetcher. Its content-link analyzer still invokes Gatekeeper's online batch
path, so offline-only log cleanup would be insufficient.

Existing extension/MCP/dashboard/batch/mail/PDF contracts and bidirectional
callback dependencies were checked. Browser routing forces its private execution
flag server-side; old scan routes keep persistence/polling/callback behavior.
Numeric user usage is shared and deliberately retained; no new scan/history or
artifact schema migration is needed. Backend's existing usage PR #20 is a source
prerequisite. Log-field removal affects operational log consumers, documented in
the seven provider PRs; response evidence/verdict fields remain unchanged.

Implementation order and deployment order are separate in
[rollout](cross-repository-rollout.md). Providers must reach the intended runtime
before orchestrator/backend and clients. Source review does not prove active
feature flags or deployment state.

## Refresh evidence and limitations

The login-error follow-up refreshed browser/dashboard code graphs, browser and
clients components, then workspace: 13,045 nodes and 28,726 edges, zero aggregate
dangling endpoints. `BrowserAccount setupBrowserAuthRoutes RequestError` was
repeated and found 81 nodes (display budget truncated). Source confirms only
Android error presentation and dashboard reconciliation with main changed;
backend contracts and deployment dependencies remain unchanged. The semantic
document/other-branch/external-source limitations below still apply.

The account-pairing implementation refreshed backend and browser repo-local code
graphs plus the dashboard worktree graph, rebuilt backend/browser/clients
components, then rebuilt the workspace: 13,035 nodes, 28,721 edges and zero
aggregate dangling endpoints. The browser component has 489 nodes and drops 146
external/library AST endpoints. The original account query
`ExtensionAuthService CreateBrowserScan RefreshAccessToken PhiSharkBridge`
returned 209 nodes; the original profile/persistence/callback query returned 593.
Both displayed results were budget-truncated and checked against current source.
The clients rebuild drops an old semantic hyperedge because this refresh is
code-only. Document semantics, other-branch snapshots and external Chromium
remain incomplete. New account consumers are the dashboard pairing route and
native browser; old extension/mobile routes retain their contracts. Backend
must precede dashboard and native rollout. Firestore revocation-collection
permissions, live callback behavior and active flags remain unknown.

The subsequent login/branding review refreshed the browser code/component and
workspace graphs: 390 browser nodes, 13,002 workspace nodes, 28,695 workspace
edges, zero aggregate dangling endpoints. The browser component drops 113
external/library AST endpoints. Repeated queries were
`setupMobileAuthRoutes refresh token PKCE Playwright continue override browser warning`
(303 nodes), `PhiSharkBridge first_run refresh` (188 nodes), and the original
profile/callback query (451 nodes); displayed results were budget-truncated.
Source review verified the separate mobile-auth dashboard branch, fixed mobile
callback, API-key-only browser routes and refresh rotation dependency. No auth
contract was migrated in this review. The edits affect Android UI/overlay and
documentation only; the active external ARM build was not changed. Document
semantics and other-branch snapshots retain the limitations below.

Affected repo-local code graphs were extracted/updated without LLM extraction.
Their intermediate snapshots point to the isolated feature worktrees and exact
current source rather than silently replacing the dirty canonical checkout.
Affected backend, orchestrator, browser and Gatekeeper components were rebuilt.
The grouped analyzers component was rebuilt from its 21 module snapshots after
refreshing the six changed analyzer modules. The aggregate workspace graph was
then rebuilt and the original query rerun.

The subsequent Android overlay/source/fixture rebuild produced a workspace graph of 12,991
nodes and 28,680 edges, with zero post-build dangling endpoints. The broad original query
was repeated against this refreshed graph and found 451 connected nodes; its CLI
output was budget-truncated. Narrow source queries, rather than absence from the
truncated output, determined the actual edit scope.

The Mac handoff also repeated `iOS baseline bootstrap Fennec Swift Keychain`
before and after the browser/component/workspace refresh. Its source-verified
change scope is limited to the browser's own scripts and documentation; no
server contract or imported upstream source was changed for this handoff.

Android implementation also queried `NavigationThrottle PhiShark ApiKeyVault
browser preflight native` before editing and after the browser/component/workspace
refresh. The refreshed query found 197 connected nodes, including the native
overlay and JNI/vault relationship; output was truncated at 1,000 tokens.
The browser component drops 111 unresolved external/library AST endpoints.
The external Chromium checkout is outside the graph; its overlay was applied
after the local x64 baseline launch passed. Current source/headers and separate native
compile checks, rather than graph absence, determine these integration points.
The actual launch test additionally exposed Cromite's internal firewall: source
review of `FirewallService::IsAllowed` and its generated annotation/rules map
established its default-deny behavior. The overlay adds only PhiShark's traffic
annotation and matching allow rule; that external engine relationship is not
represented by the aggregate code graph. Device verification remains required.

This impact check remains **incomplete for document semantics and unaffected
branch snapshots**. Code-only updates do not refresh semantic document extraction;
some pre-existing backend/orchestrator documents were omitted by earlier graph
generation. Unchanged members retain snapshots from other working branches.
The graph warns about its older pre-#1504 node-ID scheme and potential same-name
collisions. Four browser external/concept nodes lack source_file metadata.
Component construction drops unresolved AST references to external/library
symbols (Gatekeeper had 232; grouped analyzers 619). Zero aggregate dangling
endpoints therefore does not mean complete source coverage or absence of
dependencies. Old semantic hyperedges were not represented by the refreshed
code-only Gatekeeper extraction.

No known API consumer was intentionally left with an incompatible migration.
Remaining rollout/privacy risks are the usage prerequisite, provider log-field
consumers, unknown runtime/proxy/retention settings and external-provider terms.
Native navigation consumers and platform callback behavior remain unverified
until real platform/device tests. Do not declare MVP acceptance or full graph
coverage from this refresh.

## Production rollout follow-up — 2026-10-09

All seven provider deployments, orchestrator, both backend prerequisite/browser
deployments and dashboard deployment completed successfully through their official
workflows. The [production report](production-rollout-20261009.md) records exact
merge SHAs/run links and 5/5 public smoke checks. The usage rollout prerequisite
is now deployed; authenticated scan/retention and human login acceptance remain
unverified. Public auth start success does not prove enabled deep/prompt runtime
flags or persistence behavior.

After the documentation update, the browser code graph was refreshed (509 nodes,
1,046 raw edges), its component rebuilt (891 edges), and the workspace rebuilt
(13,063 nodes / 28,748 edges, zero aggregate dangling endpoints). The original
`browser account deployment BROWSER_SCAN_INTERNAL_TOKEN deploy-self-hosted`
query was repeated and found 734 connected nodes; displayed output was truncated
at 600 tokens. The browser component drops 146 unresolved external AST endpoints.
Code coverage, stale semantic documents, older node IDs, four external concept
nodes without source_file, mixed branch snapshots and live-runtime limits above
still apply. No full semantic rebuild or runtime coverage is claimed.
