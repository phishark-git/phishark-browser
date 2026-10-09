# PhiShark Browser

Read docs/security-architecture.md and docs/implementation-status.md before changes.
Preserve upstream navigation, storage, downloads and permission semantics.
Do not embed shared credentials. Native API credentials must never enter renderers.
Private browsing sends URL-only checks and uses isolated memory-only caches.
Only the ephemeral /api/v1/browser/* routes may be used by this browser.
Do not fall back to existing persistent scan endpoints.
Do not claim full redirect or prompt-injection parity without platform tests.
Obtain unchanged upstream baseline builds before modifying platform upstream trees.
Keep Android GPL and iOS MPL obligations separate; retain every upstream notice.
Run npm test and platform-native tests before release. Android builds run on Linux.
Do not deploy providers, publish releases or upload to stores without approval.

Upstream updates: docs/upstream-update-strategy.md. No automatic upstream merge.
