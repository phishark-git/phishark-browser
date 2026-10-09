# Gatekeeper navigation routing

9 October 2026. Previously Android captured every eligible non-blocked loaded
page, including targets already allowed by Gatekeeper. Explicit allow now skips
HTML capture and deep analysis; a low fast-analysis score alone does not.

## Cross-repository plan and compatibility

The workspace Graphify query `browser preflight gatekeeper whitelist unknown
blacklist` was checked against SYSTEM_MAP.yaml and current source. Provider:
Gatekeeper offline lookup → orchestrator profile execution → public backend
browser result. Consumers: Android native session/capture observer, shared JS
reference, Swift security package and the Mac navigation adapter. Extension and
Playwright use separate paths and are unchanged.

Orchestrator `startProfileScanInternal` already calls `CheckTargetOffline` and
short-circuits preflight allow/deny with `gatekeeper_benign:*` or
`gatekeeper_malicious:*`. Backend `createProfileScan` preserves this field in the
ephemeral response. No provider, API, persistence, callback, usage-counter or
deployment-workflow change is necessary. Browser is the only edited repository.

Implementation order: shared/C++/Swift routing → Android capture gate → tests,
build and emulator checks → Mac handoff. Deployment order: no server deployment;
each verified mobile client can be updated separately. Store publication is not
part of this change. Local source does not prove live provider/flag state.

## Contract

| Preflight outcome | Behavior |
| --- | --- |
| Completed, non-degraded explicit Gatekeeper allow with safe score | Allow; no HTML capture/deep; checking indicator ends |
| Same explicit allow with benign/safe/allowed verdict and no numeric analysis | Same behavior; do not invent a score |
| Unknown with low fast-analysis score | Load, then one consented non-private deep scan |
| Gatekeeper deny, definitive threat or score ≥86 | Block; no deep |
| Provisional warning | Existing consented deep and risk policy |
| Degraded, failed, malformed or conflicting allow result | Never grant the allowlist bypass |

The explicit allow uses existing server-owned `short_circuit_reason`; page text,
low score and benign verdict alone cannot grant it. Invalid numeric types stay
unverified. A completed Gatekeeper short circuit can omit numeric analysis.
Private mode remains URL-only. Deep results still use the combined decision and
never add preflight score or start another scan after completion.

Allow is scoped to the navigation generation. Each redirect or changed path/query
gets its own check; stale allow cannot trust the new target. Cache hits use the
same routing. Duplicate fragment callbacks retain their generation. Blocks stay
sticky. Existing last-safe-page bookkeeping is retained; pre-navigation allow
does not certify a committed page.

## Validation

JS: 10 tests passed, including real loopback HTTP routing/counts, stale navigation,
private mode, invalid responses, cancellation, cache and retry regression checks.
C++: 58 shared decision/routing vectors and navigation invariants passed.
Fixtures cover whitelist 1 preflight / 0 deep, unknown 1 / 1, blacklist 1 / 0
without a page GET, and a whitelisted redirect to an independently checked target.
Android APK/AAB and emulator checks are recorded below.
Swift tests are prepared; Mac compilation, adapter integration and iPhone checks
remain pending. Unchanged server repositories were read, not retested for this
mobile-only change.

Graphify uses mixed branch snapshots and older IDs. INFERRED edges are leads.
Refreshing code/component/workspace graphs does not refresh Markdown/XML semantics.

## Android build and device evidence

Source `1e47449b`: final Chromium native compile and x64 APK/AAB packaging passed
in 3m03.06s. In-place APK installation succeeded without clearing account/app
data. Targeted API 35 x64 emulator observations and fixture-server counters:

| Scenario | Preflight POSTs | Deep POSTs | Observation |
| --- | --- | --- | --- |
| Whitelist, benign with no numeric score | 1 | 0 | Document loaded, no terminal checking indicator |
| Unknown, low preflight score | 1 | 1 | Document loaded; deep completed; indicator ended |
| Blacklist, definitive deny with low fixture score | 1 | 0 | Native block; zero blacklist document GETs; no bypass |
| Allowlisted redirect origin | 1 | 0 | Redirect proceeded; allow did not transfer |
| Unknown redirect final target | 1 | 1 | Deep score 61 blocked and replaced page with about:blank |

An additional incidental preflight-block fixture was activated while the page
layout shifted; it also blocked at score 86 with no deep. Total server counts
were 6 preflight / 2 deep, with zero privacy rejections. These extra URL checks
are separate navigations, not repeated deep scans of the whitelist page.

Return-to-safety worked. Original command-line configuration was restored,
fixture runtime flag verified off, and local server stopped. Normal app launch
was repeated. APK SHA-256:
`6e277eec6f50b51c3709062532bc60bd2cccdea25ecdcdb881a09f56dda678d4`.
AAB SHA-256:
`ad8b3bd8a979b828b0dd3cd1bff1a5284ff91f0bacd55139ccdbe09c278ce07c`.

Cache/fragment/stale/private invariants passed local tests; this run did not add
device checks for every such case, tab restore, real allowlisted public sites or
provider retention. Full mobile acceptance and Mac validation remain separate.

Browser code graph refreshed: 581 nodes / 1,196 raw edges. Browser component:
1,037 edges, with 148 external AST references omitted and four conceptual nodes
missing source paths. Workspace rebuilt: 14,139 nodes / 30,131 edges, zero
dangling aggregate endpoints. Original impact query repeated (420 nodes, output
budget truncated). Mixed branches, old IDs, semantic coverage and planned fields
in SYSTEM_MAP still limit graph completeness. No server consumer migration or
rollout dependency was introduced; Mac's native adapter remains an explicit
unverified consumer until it adopts and tests this routing.
