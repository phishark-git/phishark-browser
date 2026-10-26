// SPDX-License-Identifier: GPL-3.0-only
#ifndef CHROME_BROWSER_PHISHARK_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PHISHARK_NAVIGATION_THROTTLE_H_
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
namespace content { class NavigationThrottleRegistry; class WebContents; }
namespace phishark {
void AttachTabProtection(content::WebContents* contents);
class NavigationThrottle final : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);
  explicit NavigationThrottle(content::NavigationThrottleRegistry& registry);
  ~NavigationThrottle() override;
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;
 private:
  ThrottleCheckResult Check();
  void Complete(bool blocked);
  base::WeakPtrFactory<NavigationThrottle> weak_factory_{this};
};
}  // namespace phishark
#endif
