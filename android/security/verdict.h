// SPDX-License-Identifier: GPL-3.0-only
#ifndef PHISHARK_BROWSER_VERDICT_H_
#define PHISHARK_BROWSER_VERDICT_H_

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace phishark {
enum class Profile { kPreflight, kDeep };
enum class Verdict { kChecking, kSafe, kWarning, kBlocked, kUnverified, kServiceError };
struct Result {
  std::string profile;
  std::optional<double> score;
  std::string verdict;
  std::string short_circuit_reason;
  std::string status;
  bool degraded = false;
};
inline std::string Normalize(std::string_view input) {
  std::string value(input);
  auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
  for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return value;
}
inline Verdict Decide(Profile profile, const Result& result) {
  if (result.profile != (profile == Profile::kPreflight ? "preflight" : "browser_deep_scan"))
    return Verdict::kUnverified;
  const auto v = Normalize(result.verdict);
  const auto reason = Normalize(result.short_circuit_reason);
  if (v == "unsafe" || v == "malicious" || v == "blocked" || v == "phishing" || v == "dangerous" ||
      reason.rfind("gatekeeper_malicious:", 0) == 0 ||
      reason == "prompt_injection_detected" || reason == "prompt_injection_suspected")
    return Verdict::kBlocked;
  if ((!result.status.empty() && result.status != "completed") || !result.score ||
      !std::isfinite(*result.score) || *result.score < 0 || *result.score > 100)
    return Verdict::kUnverified;
  if (*result.score >= (profile == Profile::kPreflight ? 86 : 61)) return Verdict::kBlocked;
  if (result.degraded) return Verdict::kUnverified;
  return *result.score >= 31 ? Verdict::kWarning : Verdict::kSafe;
}

// One instance per WebContents. Browser-process ownership only; a renderer
// never holds the API key or decides whether a blocked navigation resumes.
class NavigationSession {
 public:
  explicit NavigationSession(bool private_mode) : private_mode_(private_mode) {}
  uint64_t Begin(std::string canonical_url) {
    ++generation_; url_ = std::move(canonical_url); verdict_ = Verdict::kChecking;
    warning_accepted_ = false; return generation_;
  }
  bool Apply(uint64_t generation, Profile profile, const Result& result) {
    if (generation != generation_ || verdict_ == Verdict::kBlocked) return false;
    verdict_ = Decide(profile, result);
    if (verdict_ == Verdict::kSafe && profile == Profile::kDeep) last_safe_url_ = url_;
    return true;
  }
  bool Unavailable(uint64_t generation, bool service_error = false) {
    if (generation != generation_ || verdict_ == Verdict::kBlocked) return false;
    verdict_ = service_error ? Verdict::kServiceError : Verdict::kUnverified;
    return true;
  }
  bool AcceptWarning(uint64_t generation) {
    if (generation != generation_ || verdict_ != Verdict::kWarning) return false;
    warning_accepted_ = true; return true;
  }
  bool CanCapture() const { return consent_ && !private_mode_ && verdict_ != Verdict::kBlocked; }
  void SetConsent(bool consent) { consent_ = consent; }
  void Close() { ++generation_; url_.clear(); last_safe_url_.clear(); verdict_ = Verdict::kUnverified; consent_ = false; }
  Verdict verdict() const { return verdict_; }
  const std::string& last_safe_url() const { return last_safe_url_; }
 private:
  uint64_t generation_ = 0;
  bool private_mode_, consent_ = false, warning_accepted_ = false;
  Verdict verdict_ = Verdict::kUnverified;
  std::string url_, last_safe_url_;
};
}  // namespace phishark
#endif
