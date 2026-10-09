# Per-navigation request diagnostics

Impact query: `TabProtection Preflight Capture ReuseNavigationScan deepPending deadline`.
Verify against the current overlay, not graph snapshots. Provider: browser-process
HTTP submissions and cache hits. Direct consumer: JNI protection panel and the
active-tab scanning indicator. Indirect consumers: existing backend/orchestrator
browser routes. No public payload, storage, callback, quota or server behavior
changes. iOS needs its own implementation; its current behavior is unverified.

Implementation order: native memory-only counters, JNI panel and phase labels,
documented tests, actual Chromium build and emulator checks. Deployment order:
Android APK update only. No production deployment is required.

Count actual analysis POST submissions separately for preflight and deep. Count
cache hits separately, and identify authentication/capacity retry attempts. Reset
on a new navigation generation and discard with the tab; retain no URL, evidence,
credential, scan response or persistent diagnostic log. Repeated same-document
callbacks retain the counters of their existing generation. Redirects, changed
query/path and new documents start a new generation; these counters do not claim
to measure a whole redirect chain or all tabs. Account refresh and page resource
requests are outside the analysis POST counters.

The discreet indicator distinguishes URL checking from content analysis. The
panel identifies the result's phase, so a preflight result cannot be mistaken
for a deep result. Request counts do not alter safety decisions or deadlines.

## Validation

Source `d1aebded`, x64 API 35 emulator, synthetic loopback only:

| Scenario | New preflight POSTs | New deep POSTs | Panel / indicator |
| --- | ---: | ---: | --- |
| Fresh slow-history document, five fragment/history events | 1 | 1 | URL 1 / content 1; zero retries/cache hits |
| Capacity 429 once per profile | 2 | 2 | URL 2 / content 2; two capacity retry POSTs |
| Reload the identical capacity document within cache TTL | 0 | 0 | URL 0 / content 0; one cache hit per profile; retry counters reset |
| Stalled response bodies | 1 | 1 | URL-check label observed; indicator absent at a 31.5-second observation after submission |
| Second fresh slow-history URL | 1 | 1 | Content-analysis label observed while pending; hidden after completion |

Final fixture totals: 5 preflight / 5 deep POSTs; zero privacy rejections. The
stall test sampled the UI before/after the two sequential 10/20-second client
deadlines; it is not a precision timing measurement. Page loading is outside
those individual API deadlines. Live production request totals and downstream
analyzer calls were not inspected. Cross-tab coalescing and iOS remain incomplete.
Authentication-retry counting compiled but was not exercised with a real session
expiry in this test. The panel is a snapshot when opened, not a live dashboard.

8 shared tests, 42 C++ decision vectors/navigation invariants, 19 OAuth assertions
and 5 branding tests passed. Actual Chromium C++/Java/JNI compilation and x64
APK/AAB packaging passed in 4m34.53s. The existing upstream Android XR
`UsedByNative` warning remained nonfatal. In-place APK installation and launch
passed without clearing app data; the connected account remained visible in the
panel. Original command-line settings were restored and fixture mode verified
off; the test server was stopped. Full mobile acceptance remains pending.

APK SHA-256: `32e6dab81ae85e5e6c806fee8a69ad1063d43a3605fc3ac788e79ff67aaf4e8b`.
AAB SHA-256: `2e6752a640026d705561a05139e53e995484bb4b368daaf27848deef2edbb72e`.

Browser code graph, grouped component and workspace graph refreshed; the initial
query was repeated and found 171 nodes (budget truncated). Browser: 557 nodes /
1,125 raw edges; component: 969 edges; workspace: 13,111 nodes / 28,826 edges,
with zero dangling aggregate endpoints. Component generation excludes 145
external AST references; four conceptual nodes lack source files. The code-only
refresh does not refresh Markdown semantics. Mixed branch snapshots, older node
IDs and outdated planned fields in `SYSTEM_MAP.yaml` remain supporting-evidence
limitations. Current source and the actual device tests establish this Android
change; no server contract or deployment change, nor validated iOS migration, is
claimed. The Mac handoff describes the pending independent implementation.
