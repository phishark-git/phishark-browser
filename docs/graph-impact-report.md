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

Affected repo-local code graphs were extracted/updated without LLM extraction.
Their intermediate snapshots point to the isolated feature worktrees and exact
current source rather than silently replacing the dirty canonical checkout.
Affected backend, orchestrator, browser and Gatekeeper components were rebuilt.
The grouped analyzers component was rebuilt from its 21 module snapshots after
refreshing the six changed analyzer modules. The aggregate workspace graph was
then rebuilt and the original query rerun.

The final source/fixture rebuild produced a workspace graph of 12,814 nodes and
28,339 edges, with zero post-build dangling endpoints. The broad original query
was repeated against this refreshed graph and found 451 connected nodes; its CLI
output was budget-truncated. Narrow source queries, rather than absence from the
truncated output, determined the actual edit scope.

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
