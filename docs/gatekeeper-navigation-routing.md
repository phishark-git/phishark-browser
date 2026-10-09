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
Android APK/AAB and emulator checks are recorded after execution below.
Swift tests are prepared; Mac compilation, adapter integration and iPhone checks
remain pending. Unchanged server repositories were read, not retested for this
mobile-only change.

Graphify uses mixed branch snapshots and older IDs. INFERRED edges are leads.
Refreshing code/component/workspace graphs does not refresh Markdown/XML semantics.
