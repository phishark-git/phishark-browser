// SPDX-License-Identifier: GPL-3.0-only
#include "verdict.h"
#include "vectors.inc"
#include <cassert>
#include <iostream>

int main() {
  using namespace phishark;
  for (const auto& vector : vectors) assert(Decide(vector.profile, vector.result) == vector.expected);
  NavigationSession normal(false), private_tab(true);
  normal.SetConsent(true); private_tab.SetConsent(true);
  const auto old = normal.Begin("https://first.example/");
  const auto current = normal.Begin("https://second.example/");
  Result unsafe{"preflight", 100, "malicious", "", "completed"};
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
  std::cout << vectors.size() << " native decision vectors and navigation invariants passed\n";
}
