# Playwright reference analysis

Reference: `phishark-playwright-mcp` main b0c5f80, with prompt-policy evidence from `codex/prompt-injection-evidence` 8ab6bcd. Its 15 passing unit tests are reference evidence only. They do not validate this mobile code or server changes.

`src/phishark/guard.js`, `client.js`, `network.js`, `evidence.js` and `cache.js` are the reference contract. Preflight sends `{target}`; deep sends `{target, web_evidence}`. WHATWG normalization preserves path/query, removes fragments and normalizes IDN/default ports. Private/metadata hosts and resolved private addresses are refused by the reference network guard.

Preflight definitive threat or score >=86 blocks; 31–85 is provisional warning. Deep is the final combined decision, not a second score added to preflight: 0–30 safe, 31–60 warning, >=61 blocked, negative/invalid unverified. Prompt `suspicious` and `malicious` short circuits both block. A missing prompt result never proves safety. The mobile contract conservatively marks degraded non-blocking results unverified.

The MCP's temporary-error read-only tool restriction differs intentionally from mobile: mobile navigation continues with an unverified indicator. Authentication, quota and permanent configuration failures get a separate service screen.

The reference captures raw HTML/screenshots. Mobile capture must sanitize form state, omit cookies and authorization headers, and redact sensitive screenshot regions in the native capture path. Prompt-policy text extraction is bounded to four 2,500-character chunks, 10,000 total, with structured content including hidden instructions and frame evidence. This extraction is a reference for pending native integration, not proof that the current mobile files collect it.

The workspace graph initially includes different active branches and old/main checkouts. In particular, provider work starts from backend 6df894b and orchestrator b297500 rather than stale main. Graph edges marked INFERRED/AMBIGUOUS are leads; current source determines the contracts. No deployed flags or runtime topology were inspected.
