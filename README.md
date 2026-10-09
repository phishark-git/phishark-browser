# PhiShark Browser

An open-source Android and iOS browser built on Cromite/Chromium and Firefox iOS/WebKit.

**Development status:** this repository is under implementation. It is not yet a
verified, releasable mobile browser. See [implementation status](docs/implementation-status.md).

PhiShark checks document navigations before loading and, with consent in normal
mode, analyzes captured page evidence after loading. Private mode performs URL-only
checks. Primary onboarding connects a PhiShark account through extension-style
PKCE pairing and remembers the native session. Personal API keys remain a developer
option; no privileged server credentials are shipped. Android account integration
is being validated; iOS still needs its native adapter and Mac acceptance.
Temporary service failures allow browsing with an explicit unverified indicator.
Confirmed threats cannot be bypassed.

Browser requests use separate ephemeral PhiShark routes, not persistent scan APIs.
Backend rollout is required before cloud protection works. No production rollout
or store publication is part of repository setup.

## Development

`npm test` checks the shared contract, privacy boundaries and navigation lifecycle.
Android baseline/build steps: [android/README.md](android/README.md).
iOS baseline/build steps: [ios/README.md](ios/README.md).

Start with [security architecture](docs/security-architecture.md),
[Playwright analysis](docs/playwright-mcp-analysis.md) and
[upstream strategy](docs/upstream-update-strategy.md).

Licenses are component-specific: Android GPL, iOS MPL and shared Apache-2.0.
Retain upstream copyrights and corresponding-source obligations.
