// SPDX-License-Identifier: GPL-3.0-only
#include "verdict.h"
#include "vectors.inc"
#include <cassert>
#include <iostream>

int main() {
  using namespace phishark;
  assert(!ReuseNavigationScan(0, 1, "https://a/", 1, "https://a/", false, false));
  assert(ReuseNavigationScan(1, 1, "https://a/", 1, "https://a/", false, false));
  for (int i = 2; i <= 6; ++i)
    assert(ReuseNavigationScan(1, 1, "https://a/", i, "https://a/", true, true));
  assert(!ReuseNavigationScan(1, 1, "https://a/", 2, "https://a/", true, false));
  assert(!ReuseNavigationScan(1, 1, "https://a/", 2, "https://a/", false, true));
  assert(!ReuseNavigationScan(1, 1, "https://a/", 1, "https://a/redirect", false, false));
  assert(!ReuseNavigationScan(1, 1, "https://a/", 2, "https://a/?q=changed", true, true));
  for (const auto& vector : vectors) {
    assert(Decide(vector.profile, vector.result) == vector.expected);
    assert(TrustedPreflight(vector.result) == vector.skip_deep);
  }
  NavigationSession normal(false), private_tab(true);
  normal.SetConsent(true); private_tab.SetConsent(true);
  const auto old = normal.Begin("https://first.example/");
  const auto current = normal.Begin("https://second.example/");
  Result unsafe{"preflight", 100, "malicious", "", "completed"};
  Result invalid_threat{"preflight", NAN, "malicious", "", "completed"};
  assert(Decide(Profile::kPreflight, invalid_threat) == Verdict::kBlocked);
  Result safe{"browser_deep_scan", 0, "safe", "", "completed"};
  assert(!normal.Apply(old, Profile::kPreflight, unsafe));
  assert(normal.Apply(current, Profile::kDeep, safe));
  assert(normal.last_safe_url() == "https://second.example/");
  assert(!private_tab.CanCapture());
  assert(normal.Apply(current, Profile::kPreflight, unsafe));
  assert(!normal.AcceptWarning(current));
  assert(!normal.Apply(current, Profile::kDeep, safe));
  assert(!normal.CanCapture());
  normal.Close(); assert(normal.last_safe_url().empty());
  NavigationSession partial(false);
  Result url_safe{"preflight", 0, "safe", "", "completed"};
  auto first = partial.Begin("https://public.example/");
  assert(partial.Apply(first, Profile::kPreflight, url_safe));
  assert(partial.Unavailable(first));
  assert(partial.verdict() == Verdict::kUnverified);
  assert(partial.url_verdict() == Verdict::kSafe);
  assert(partial.last_safe_url().empty()); // URL-only never certifies the page.
  auto second = partial.Begin("https://next.example/");
  assert(partial.url_verdict() == Verdict::kUnverified);
  assert(!partial.Apply(first, Profile::kPreflight, url_safe));
  assert(partial.url_verdict() == Verdict::kUnverified);
  assert(partial.Apply(second, Profile::kPreflight, url_safe));
  Result deep_block{"browser_deep_scan", 100, "malicious", "", "completed"};
  assert(partial.Apply(second, Profile::kDeep, deep_block));
  assert(partial.verdict() == Verdict::kBlocked);
  assert(!partial.Unavailable(second)); // Saved URL context cannot bypass a block.
  partial.Close(); assert(partial.url_verdict() == Verdict::kUnverified);
  NavigationSession trusted(false);
  trusted.SetConsent(true);
  Result whitelist{"preflight", 0, "benign", "gatekeeper_benign:whitelist", "completed"};
  auto allowed = trusted.Begin("https://allowed.example/");
  assert(trusted.Apply(allowed, Profile::kPreflight, whitelist));
  assert(!trusted.CanCapture());
  assert(trusted.last_safe_url().empty()); // Pre-navigation allow does not certify a committed page.
  auto redirect = trusted.Begin("https://unknown.example/");
  assert(!trusted.Apply(allowed, Profile::kPreflight, whitelist));
  assert(trusted.Apply(redirect, Profile::kPreflight, url_safe));
  assert(trusted.CanCapture()); // An allowlisted origin never trusts its redirect.
  assert(trusted.Apply(redirect, Profile::kDeep, deep_block));
  assert(!trusted.CanCapture());
  trusted.Close(); assert(trusted.last_safe_url().empty());
  std::cout << vectors.size() << " native decision vectors and navigation invariants passed\n";
}
