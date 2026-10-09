# PhiShark Android branding

The launcher and account onboarding use the existing PhiShark mark from
`website-dashboard-frontend/public/phishark_logo.png`. The wordmark comes from
`public/logo/main-logo.png` in the same repository. Both are copied unchanged,
not generated approximations. They remain PhiShark brand assets.

Product messages are branded at the pinned Cromite GRIT text hook. Legal,
copyright, license and third-party notices are excluded; resource names,
package internals, browser schemes and website links are not renamed. A
separate native account callback owns the OAuth code exchange. No website
renderer receives account tokens or the PKCE verifier.

First-run copy, launcher label, icon, security UI and account onboarding identify
PhiShark Browser. General tab/navigation animations remain upstream. Search
provider logos identify the selected search provider and are not browser marks.
Factory new-tab suggestions point to PhiShark and the account dashboard. Existing
user history and bookmarks are not rewritten. The security badge retains its
220 ms state fade and the PhiShark navy/turquoise palette.
All surfaces still require visual checks in the rebuilt APK, including dark mode,
adaptive/monochrome icons, splash, new tab, settings and accessibility scaling.
