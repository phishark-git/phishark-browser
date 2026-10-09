// SPDX-License-Identifier: GPL-3.0-only
#include "chrome/browser/phishark/masked_screenshot.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/android/view_android.h"

namespace phishark {
namespace {
constexpr size_t kMaxProtocolBytes = 16 * 1024 * 1024;
constexpr size_t kMaxImageBytes = 2 * 1024 * 1024;
constexpr size_t kMaxNodes = 100000;
constexpr size_t kMaxMasks = 4096;

gfx::Rect VisibleSurfaceRect(content::WebContents* contents, double page_scale) {
  auto* widget = contents->GetRenderWidgetHostView();
  auto* view = widget ? widget->GetNativeView() : nullptr;
  if (!view) return {};
  const auto size = view->viewport_size();
  // Android frame metadata reports the scrollable viewport before page zoom.
  // Convert it back to physical compositor pixels using this frame's current
  // page scale and the actual device density, rather than a fixed screen size.
  const double scale = view->GetDipScale() * page_scale;
  // CopyFromSurface reads the renderer's local surface, whose document begins
  // at (0,0). ViewAndroid content_offset translates it in the browser UI only;
  // applying that translation here cuts off the page and shifts every mask.
  const double x = 0, y = 0;
  const double width = size.width() * scale, height = size.height() * scale;
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width)
      || !std::isfinite(height) || scale <= 0 || x < 0 || y < 0
      || width < 1 || height < 1 || x + width > 4096 || y + height > 4096) return {};
  // Exclude toolbar reservation and edge pixels outside the visible document.
  const int left = std::ceil(x), top = std::ceil(y);
  return gfx::Rect(left, top, std::floor(x + width) - left,
                   std::floor(y + height) - top);
}

std::optional<double> Number(const base::Value* value) {
  if (!value || (!value->is_double() && !value->is_int())) return std::nullopt;
  double number = value->is_double() ? value->GetDouble() : value->GetInt();
  if (!std::isfinite(number)) return std::nullopt;
  return number;
}
const std::string* StringAt(const base::ListValue& strings,
                            const base::Value& value) {
  // Blink AddString uses -1 for an empty string, including boolean attributes.
  static const base::NoDestructor<std::string> empty;
  if (value.is_int() && value.GetInt() == -1) return empty.get();
  if (!value.is_int() || value.GetInt() < 0 ||
      static_cast<size_t>(value.GetInt()) >= strings.size()) return nullptr;
  return strings[value.GetInt()].GetIfString();
}
}  // namespace

ScreenshotCapture::ScreenshotCapture(content::WebContents* contents, GURL target,
                                     Completion completion)
    : host_(content::DevToolsAgentHost::GetOrCreateFor(contents)), contents_(contents),
      target_(std::move(target)), completion_(std::move(completion)) {}
ScreenshotCapture::~ScreenshotCapture() {
  if (attached_) host_->DetachClient(this);
}
bool ScreenshotCapture::MayAttachToURL(const GURL& url, bool is_webui) {
  return !is_webui && url.GetWithoutRef() == target_;
}
bool ScreenshotCapture::MayReadLocalFiles() { return false; }
bool ScreenshotCapture::IsTrusted() { return false; }
bool ScreenshotCapture::MayWriteLocalFiles() { return false; }
bool ScreenshotCapture::MayAccessAllCookies() { return false; }
void ScreenshotCapture::Start() {
  // Do not disturb a user's existing debugger session.
  if (!host_ || host_->IsAttached() || !host_->AttachClient(this)) {
    Finish(); return;
  }
  attached_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(FROM_HERE,
      base::BindOnce(&ScreenshotCapture::Finish, weak_factory_.GetWeakPtr(),
                     MaskedScreenshot()), base::Seconds(6));
  // Android browser controls settle just after document onload. Capture their
  // final viewport without triggering an additional analysis HTTP request.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(FROM_HERE,
      base::BindOnce(&ScreenshotCapture::Send, weak_factory_.GetWeakPtr(),
                     std::string("Page.getLayoutMetrics"), base::DictValue()),
      base::Milliseconds(350));
}
void ScreenshotCapture::Send(std::string method, base::DictValue parameters) {
  base::DictValue message;
  message.Set("id", ++command_); message.Set("method", std::move(method));
  message.Set("params", std::move(parameters));
  auto json = base::WriteJson(message);
  if (!json) { Finish(); return; }
  host_->DispatchProtocolMessage(this, base::as_byte_span(*json));
}
void ScreenshotCapture::AgentHostClosed(content::DevToolsAgentHost*) {
  attached_ = false; Finish();
}
void ScreenshotCapture::DispatchProtocolMessage(content::DevToolsAgentHost*,
                                                base::span<const uint8_t> bytes) {
  if (!completion_) return;
  if (bytes.size() > kMaxProtocolBytes) { Finish(); return; }
  auto value = base::JSONReader::ReadDict(
      std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
      base::JSON_PARSE_RFC);
  if (!value) { Finish(); return; }
  if (value->FindInt("id") != command_) return;  // Ignore protocol events.
  auto* result = value->FindDict("result");
  if (!result || value->contains("error")) { Finish(); return; }
  switch (command_) {
    case 1:
      if (!ReadViewport(*result)) { Finish(); return; }
      { base::DictValue params; params.Set("computedStyles", base::ListValue());
        Send("DOMSnapshot.captureSnapshot", std::move(params)); }
      break;
    case 2:
      if (!ReadMasks(*result)) { Finish(); return; }
      { auto* frame = contents_->GetPrimaryMainFrame();
        if (!frame) { Finish(); return; }
        command_ = 3;
        // Wait for this document's visual state rather than copying an old
        // surface from the preceding navigation.
        frame->InsertVisualStateCallback(base::BindOnce(
            &ScreenshotCapture::CopySurface, weak_factory_.GetWeakPtr())); }
      break;
    case 4:
      if (!ReadViewport(*result)) { Finish(); return; }
      { base::DictValue params; params.Set("computedStyles", base::ListValue());
        Send("DOMSnapshot.captureSnapshot", std::move(params)); }
      break;
    case 5:
      if (!ReadMasks(*result)) { Finish(); return; }
      Redact(); break;
    default: Finish();
  }
}
bool ScreenshotCapture::ReadViewport(const base::DictValue& result) {
  const auto* raw = result.FindDict("cssVisualViewport");
  const auto* physical = result.FindDict("visualViewport");
  if (!raw || !physical) return false;
  base::DictValue view;
  for (const auto* field : {"pageX", "pageY", "offsetX", "offsetY", "clientWidth", "clientHeight", "scale"}) {
    auto number = Number(raw->Find(field)); if (!number) return false;
    view.Set(field, *number);
  }
  auto zoom = Number(raw->Find("zoom")).value_or(1);
  view.Set("zoom", zoom);
  if (*view.FindDouble("clientWidth") < 1 || *view.FindDouble("clientHeight") < 1
      || *view.FindDouble("clientWidth") > 4096 || *view.FindDouble("clientHeight") > 4096
      || zoom <= 0 || zoom > 5 || *view.FindDouble("scale") <= 0) return false;
  // Blink's LayoutZoomFactor includes device density and browser zoom. The
  // protocol's physical/CSS viewport ratio exposes that exact conversion;
  // pinch zoom is applied separately. Cropping/rounding a bitmap must never
  // stretch the mask coordinates to fit its dimensions.
  auto physical_width = Number(physical->Find("clientWidth"));
  auto physical_height = Number(physical->Find("clientHeight"));
  if (!physical_width || !physical_height) return false;
  double css_to_physical = *physical_width / *view.FindDouble("clientWidth");
  double css_to_physical_y = *physical_height / *view.FindDouble("clientHeight");
  double css_to_surface = css_to_physical * *view.FindDouble("scale");
  if (css_to_surface <= 0 || css_to_surface > 32
      || std::abs(css_to_physical - css_to_physical_y) > 0.001) return false;
  view.Set("cssToSurfaceScale", css_to_surface);
  if (command_ == 4) { viewport_after_ = std::move(view); return viewport_after_ == viewport_; }
  viewport_ = std::move(view); return true;
}
bool ScreenshotCapture::ReadMasks(const base::DictValue& result) {
  const auto* documents = result.FindList("documents");
  const auto* strings = result.FindList("strings");
  if (!documents || documents->empty() || !strings) return false;
  const auto* document = (*documents)[0].GetIfDict();
  if (!document) return false;
  const auto* url_index = document->Find("documentURL");
  const auto* url = url_index ? StringAt(*strings, *url_index) : nullptr;
  if (!url || GURL(*url).GetWithoutRef() != target_) return false;
  const auto* nodes = document->FindDict("nodes");
  const auto* layout = document->FindDict("layout");
  if (!nodes || !layout) return false;
  const auto* names = nodes->FindList("nodeName");
  const auto* parents = nodes->FindList("parentIndex");
  const auto* attributes = nodes->FindList("attributes");
  const auto* ids = nodes->FindList("backendNodeId");
  const auto* indexes = layout->FindList("nodeIndex");
  const auto* bounds = layout->FindList("bounds");
  if (!names || names->empty() || names->size() > kMaxNodes || !parents ||
      parents->size() != names->size() || !attributes || attributes->size() != names->size()
      || !ids || ids->empty() || !(*ids)[0].is_int() || !indexes || !bounds ||
      indexes->size() != bounds->size()) return false;
  int document_id = (*ids)[0].GetInt();
  std::vector<bool> sensitive(names->size(), false);
  for (size_t i = 0; i < names->size(); ++i) {
    const auto* name = StringAt(*strings, (*names)[i]);
    const auto* attrs = (*attributes)[i].GetIfList();
    if (!name || name->empty() || !attrs || attrs->size() % 2) return false;
    sensitive[i] = *name == "INPUT" || *name == "TEXTAREA" || *name == "SELECT"
        || *name == "IFRAME" || *name == "FRAME" || name->find('-') != std::string::npos;
    for (size_t j = 0; j < attrs->size(); j += 2) {
      const auto* key = StringAt(*strings, (*attrs)[j]);
      const auto* val = StringAt(*strings, (*attrs)[j+1]);
      if (!key || key->empty() || !val) return false;
      if (*key == "contenteditable" && !base::EqualsCaseInsensitiveASCII(*val, "false"))
        sensitive[i] = true;
      if (*key == "role" && (*val == "textbox" || *val == "searchbox" || *val == "combobox"))
        sensitive[i] = true;
    }
  }
  // Flattened shadow nodes (including closed and UA trees) and their hosts.
  if (const auto* shadow = nodes->FindDict("shadowRootType")) {
    const auto* shadow_indexes = shadow->FindList("index");
    if (!shadow_indexes) return false;
    for (const auto& value : *shadow_indexes) {
      if (!value.is_int() || value.GetInt() < 0 ||
          static_cast<size_t>(value.GetInt()) >= sensitive.size()) return false;
      size_t index = value.GetInt(); sensitive[index] = true;
      if (!(*parents)[index].is_int()) return false;
      int parent = (*parents)[index].GetInt();
      if (parent >= 0 && static_cast<size_t>(parent) < index) sensitive[parent] = true;
    }
  }
  for (size_t i = 0; i < sensitive.size(); ++i) {
    if (!(*parents)[i].is_int()) return false;
    int parent = (*parents)[i].GetInt();
    if (parent >= 0) {
      if (static_cast<size_t>(parent) >= i) return false;
      sensitive[i] = sensitive[i] || sensitive[parent];
    }
  }
  base::ListValue masks;
  double x = *viewport_.FindDouble("pageX") + *viewport_.FindDouble("offsetX");
  double y = *viewport_.FindDouble("pageY") + *viewport_.FindDouble("offsetY");
  double width = *viewport_.FindDouble("clientWidth"), height = *viewport_.FindDouble("clientHeight");
  // DOMSnapshot RectInDocument returns Blink physical layout pixels, including
  // LayoutZoomFactor (device density/browser zoom), before pinch zoom. Convert
  // to the same CSS space as Page.cssVisualViewport before clipping/padding.
  const double physical_to_css = *viewport_.FindDouble("scale")
      / *viewport_.FindDouble("cssToSurfaceScale");
  for (size_t i = 0; i < indexes->size(); ++i) {
    const auto& index = (*indexes)[i];
    if (!index.is_int() || index.GetInt() < 0 ||
        static_cast<size_t>(index.GetInt()) >= sensitive.size()) return false;
    if (!sensitive[index.GetInt()]) continue;
    const auto* rect = (*bounds)[i].GetIfList();
    if (!rect || rect->size() != 4) return false;
    auto rx = Number(&(*rect)[0]), ry = Number(&(*rect)[1]);
    auto rw = Number(&(*rect)[2]), rh = Number(&(*rect)[3]);
    if (!rx || !ry || !rw || !rh || *rw < 0 || *rh < 0) return false;
    *rx *= physical_to_css; *ry *= physical_to_css;
    *rw *= physical_to_css; *rh *= physical_to_css;
    if (*rw == 0 || *rh == 0) continue;
    // Outward padding protects glyph antialiasing and native control edges.
    double left = std::clamp(*rx - x - 8, 0.0, width);
    double top = std::clamp(*ry - y - 8, 0.0, height);
    double right = std::clamp(*rx + *rw - x + 8, 0.0, width);
    double bottom = std::clamp(*ry + *rh - y + 8, 0.0, height);
    if (right <= left || bottom <= top) continue;
    if (masks.size() == kMaxMasks) return false;
    base::ListValue region; region.Append(left); region.Append(top);
    region.Append(right); region.Append(bottom); masks.Append(std::move(region));
  }
  if (command_ == 5) return document_id_ == document_id && masks_ == masks;
  document_id_ = document_id; masks_ = std::move(masks); return true;
}
void ScreenshotCapture::CopySurface(bool visual_state_ready) {
  if (!completion_) return;
  auto* view = contents_->GetRenderWidgetHostView();
  if (!visual_state_ready || !view || contents_->GetVisibility() != content::Visibility::VISIBLE
      || contents_->GetLastCommittedURL().GetWithoutRef() != target_) { surface_failure_ = 1; Finish(); return; }
  surface_clip_ = VisibleSurfaceRect(contents_, *viewport_.FindDouble("scale"));
  if (surface_clip_.IsEmpty()) { surface_failure_ = 2; Finish(); return; }
  // Read the actual compositor surface without changing viewport/emulation.
  view->CopyFromSurface(gfx::Rect(), gfx::Size(), base::Seconds(2),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ScreenshotCapture::SurfaceCaptured, weak_factory_.GetWeakPtr())));
}
void ScreenshotCapture::SurfaceCaptured(const content::CopyFromSurfaceResult& result) {
  if (!completion_) return;
  if (!result.has_value() || result->bitmap.drawsNothing()) { surface_failure_ = 3; Finish(); return; }
  const auto& source = result->bitmap;
  raw_surface_size_ = gfx::Size(source.width(), source.height());
  // Frame metadata can round a fractional device pixel outward. Restrict the
  // requested viewport to pixels actually returned by the renderer surface.
  gfx::Rect clipped = surface_clip_;
  clipped.Intersect(gfx::Rect(source.width(), source.height()));
  auto w = clipped.width(), h = clipped.height();
  if (VisibleSurfaceRect(contents_, *viewport_.FindDouble("scale")) != surface_clip_) { surface_failure_ = 4; Finish(); return; }
  if (w < 1 || h < 1 || w > 4096 || h > 4096 || uint64_t(w) * h > 12 * 1024 * 1024
      || !bitmap_.tryAllocN32Pixels(w, h)
      || !source.readPixels(bitmap_.info(), bitmap_.getPixels(), bitmap_.rowBytes(),
                            clipped.x(), clipped.y())) { surface_failure_ = 5; Finish(); return; }
  Send("Page.getLayoutMetrics");
}
void ScreenshotCapture::Redact() {
  auto& bitmap = bitmap_;
  if (bitmap.drawsNothing() || VisibleSurfaceRect(contents_, *viewport_.FindDouble("scale")) != surface_clip_) { Finish(); return; }
  double sx = *viewport_.FindDouble("cssToSurfaceScale");
  double sy = sx;
  SkCanvas canvas(bitmap); SkPaint paint;
  paint.setColor(SK_ColorBLACK); paint.setBlendMode(SkBlendMode::kSrc);
  MaskedScreenshot output; output.width = bitmap.width(); output.height = bitmap.height();
  for (const auto& value : masks_) {
    const auto& rect = value.GetList();
    int left = std::clamp(int(std::floor(rect[0].GetDouble() * sx)), 0, bitmap.width());
    int top = std::clamp(int(std::floor(rect[1].GetDouble() * sy)), 0, bitmap.height());
    int right = std::clamp(int(std::ceil(rect[2].GetDouble() * sx)), 0, bitmap.width());
    int bottom = std::clamp(int(std::ceil(rect[3].GetDouble() * sy)), 0, bitmap.height());
    canvas.drawIRect(SkIRect::MakeLTRB(left, top, right, bottom), paint);
    base::ListValue region; region.Append(left); region.Append(top);
    region.Append(right); region.Append(bottom); output.regions.Append(std::move(region));
  }
  auto png = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true);
  if (!png || png->size() > kMaxImageBytes) { Finish(); return; }
  output.png_base64 = base::Base64Encode(*png);
  Finish(std::move(output));
}
void ScreenshotCapture::Finish(MaskedScreenshot result) {
  if (!completion_) return;
  if (result.png_base64.empty()) result.failure_step = command_;
  result.viewport_before = viewport_.Clone(); result.viewport_after = viewport_after_.Clone();
  // The caller includes this numeric metadata only for explicit local fixtures.
  result.viewport_before.Set("surfaceWidth", raw_surface_size_.width());
  result.viewport_before.Set("surfaceHeight", raw_surface_size_.height());
  result.viewport_before.Set("requestedWidth", surface_clip_.width());
  result.viewport_before.Set("requestedHeight", surface_clip_.height());
  result.viewport_before.Set("surfaceFailure", surface_failure_);
  if (result.png_base64.empty()) { result.width = bitmap_.width(); result.height = bitmap_.height(); }
  weak_factory_.InvalidateWeakPtrs(); bitmap_.reset(); masks_.clear();
  if (attached_) { attached_ = false; host_->DetachClient(this); }
  // The owner can destroy us from completion, outside protocol dispatch.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
      base::BindOnce(std::move(completion_), std::move(result)));
}
}  // namespace phishark
