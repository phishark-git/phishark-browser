// SPDX-License-Identifier: GPL-3.0-only
#ifndef CHROME_BROWSER_PHISHARK_MASKED_SCREENSHOT_H_
#define CHROME_BROWSER_PHISHARK_MASKED_SCREENSHOT_H_

#include <memory>
#include <string>
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content { class WebContents; }
namespace phishark {

struct MaskedScreenshot {
  std::string png_base64;
  base::ListValue regions;  // Pixel rectangles; test metadata only at the caller.
  int width = 0;
  int height = 0;
  int failure_step = 0;
  base::DictValue viewport_before;
  base::DictValue viewport_after;
};

// One temporary browser-process session, no network debugger or renderer key.
class ScreenshotCapture final : public content::DevToolsAgentHostClient {
 public:
  using Completion = base::OnceCallback<void(MaskedScreenshot)>;
  ScreenshotCapture(content::WebContents* contents, GURL target,
                    Completion completion);
  ~ScreenshotCapture() override;
  void Start();
  void DispatchProtocolMessage(content::DevToolsAgentHost* host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(content::DevToolsAgentHost* host) override;
  bool MayAttachToURL(const GURL& url, bool is_webui) override;
  bool IsTrusted() override;
  bool MayReadLocalFiles() override;
  bool MayWriteLocalFiles() override;
  bool MayAccessAllCookies() override;
 private:
  void Send(std::string method, base::DictValue parameters = {});
  bool ReadViewport(const base::DictValue& result);
  bool ReadMasks(const base::DictValue& result);
  void CopySurface(bool visual_state_ready);
  void SurfaceCaptured(const content::CopyFromSurfaceResult& result);
  void Redact();
  void Finish(MaskedScreenshot result = {});
  scoped_refptr<content::DevToolsAgentHost> host_;
  raw_ptr<content::WebContents> contents_;
  GURL target_;
  Completion completion_;
  int command_ = 0;
  bool attached_ = false;
  base::DictValue viewport_;
  base::DictValue viewport_after_;
  base::ListValue masks_;
  int document_id_ = 0;
  SkBitmap bitmap_;
  gfx::Rect surface_clip_;
  gfx::Size raw_surface_size_;
  int surface_failure_ = 0;  // Numeric geometry diagnostics, fixture receiver only.
  base::WeakPtrFactory<ScreenshotCapture> weak_factory_{this};
};
}  // namespace phishark
#endif
