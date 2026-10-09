# Cross-repository implementation and rollout

## Scope and consumers

Provider source bases: orchestrator b297500 (V3/prompt-policy worktree) and backend 6df894b (Firestore usage worktree). Changes live in separate `codex/browser-ephemeral-20261009` branches. Mobile work lives in `codex/browser-mvp`. Existing dirty/main checkouts were preserved.

Orchestrator owns profile execution, decision fusion, artifact persistence and callbacks. Backend consumes it, validates API keys and owns numeric usage and scan history. Browser consumes only new public ephemeral endpoints. Existing MCP, extensions, dashboard and batch/mail/PDF consumers keep their original routes/semantics. Callback dependencies are bidirectional but browser mode explicitly produces no callbacks. No migration of existing history/storage/result schemas is needed.

Implementation order: shared contract/upstream baseline → orchestrator → backend → Android integration → MacBook iOS integration → release acceptance. Server code is prepared while the unchanged Android baseline is downloading/building; major upstream changes remain gated.

Deployment order: any privacy-required shared provider changes → orchestrator → backend → mobile release. Providers' logging/retention settings and active runtime flags are unknown. Publishing a client before both new API paths are available would leave it unverified/service-unavailable, not trigger a persistent fallback.

## Proposed deployment evidence; not approval

Official workflow for both server repositories: `.github/workflows/deploy-self-hosted.yml`. Local changes add only `BROWSER_SCAN_INTERNAL_TOKEN` secret-to-env forwarding; they do not change container names, ports, network or volume configuration. Both repositories must hold the same server-only secret. Runtime values were not inspected or changed. Existing profile flags remain the gate; mobile deep additionally requires `BROWSER_DEEP_PROMPT_INJECTION_ENABLED`.

Tracked workflow names/ports: orchestrator `scan-orchestrator`, host 8080/container 8080; backend `backend`, host 8000/container 8080; both use `phishark-net`. Live bindings/routes/volumes are unknown. Review candidate-container port selection and current CI evidence before deployment. Workflow replacement can cause downtime; do not promise a no-downtime cutover.

Before approval, present exact ref/workflow, container/port/network/route conflicts from configuration/CI evidence, health `/healthz` (or repository-documented backend health route), approved local candidate and live smoke checks, data-safety requirements, unknown runtime assumptions and rollback to previous image/workflow ref. Existing workflows contain production stop/replacement/cleanup commands that require review and specific approval under the workspace gate. Do not run them just because this implementation is approved. No SSH inspection is prerequisite unless separately approved or required by the approved workflow.

Browser routes create no new database migration, volume or durable evidence store. Numeric usage writes remain intentional. Existing scans/queues must drain safely according to the existing workflow. Approved verification should cover missing/wrong/correct internal token, URL-only preflight, deep prompt blocking, no browser history/artifact/callback/log outputs, quota/refund and old routes. Use synthetic fixture URLs only. Production smoke, rollback and cleanup each require the specified approval.
