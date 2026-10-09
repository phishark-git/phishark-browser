# Cross-repository impact check

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
