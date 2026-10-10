// SPDX-License-Identifier: GPL-3.0-only
#include "chrome/browser/phishark/navigation_throttle.h"
#include "chrome/browser/phishark/masked_screenshot.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "crypto/sha2.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "net/base/load_flags.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "chrome/browser/phishark/verdict.h"
#include "chrome/browser/phishark_buildflags.h"
// Include after the JNI conversion specializations.
#include "chrome/android/chrome_jni_headers/PhiSharkBridge_jni.h"

namespace phishark {
namespace {
constexpr size_t kMaxResponseBytes = 1024 * 1024;
constexpr size_t kMaxHtmlBytes = 3 * 1024 * 1024;
constexpr char16_t kCaptureScript[] = uR"JS((() => {
  // An isolated world carries no API key. All live input values are discarded.
  const copy = document.documentElement.cloneNode(true);
  for (const element of copy.querySelectorAll('script,style,noscript,template')) element.remove();
  for (const element of copy.querySelectorAll('input,textarea,select,button,[contenteditable],[role="textbox"],[role="searchbox"],[role="combobox"]')) {
    for (const attribute of [...element.attributes]) {
      if (attribute.name !== 'type') element.removeAttribute(attribute.name);
    }
    element.replaceChildren();
  }
  // Frames and custom controls are omitted until their capture privacy is verified.
  for (const element of copy.querySelectorAll('iframe,frame')) element.remove();
  for (const element of copy.querySelectorAll('*')) {
    if (element.localName.includes('-')) {
      element.replaceChildren();
      for (const attribute of [...element.attributes]) element.removeAttribute(attribute.name);
    }
    for (const attribute of [...element.attributes]) {
      if (attribute.name.startsWith('on') || attribute.name.startsWith('data-')
          || attribute.name === 'value') element.removeAttribute(attribute.name);
    }
  }
  const html = copy.outerHTML;
  if (html.length > 1048576) return null;
  const links = [];
  for (const anchor of copy.querySelectorAll('a[href]')) {
    if (links.length >= 500) break;
    try {
      const url = new URL(anchor.getAttribute('href'), document.baseURI);
      if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password) continue;
      links.push({url: url.href, domain: url.hostname, title: (anchor.textContent || '').trim().slice(0, 300)});
    } catch {}
  }
  const domains = [...new Set(links.map(link => link.domain))];
  return {html, title: copy.querySelector('title')?.textContent || '',
    outgoing_links: {count: links.length, domains_count: domains.length, domains, links}};
})())JS";

constexpr net::NetworkTrafficAnnotationTag kTraffic =
    net::DefineNetworkTrafficAnnotation("phishark_ephemeral_browser_analysis", R"(
      semantics {
        sender: "PhiShark Browser protection"
        description: "Checks a document URL and optionally consented sanitized page evidence."
        trigger: "Main document navigation with a user-configured PhiShark API key."
        data: "Full URL, personal API authentication header, optional sanitized HTML and input-masked page PNG."
        destination: OTHER
        destination_other: "User configured PhiShark API service"
      }
      policy {
        cookies_allowed: NO
        setting: "User configures API access and separately opts into normal-mode content analysis."
        policy_exception_justification: "User operated open source browser without enterprise integration."
      })");

struct CachedResult { Result result; base::TimeTicks expires; };

bool UseLocalFixtures() {
#if BUILDFLAG(PHISHARK_ALLOW_LOOPBACK_TESTING)
  return base::CommandLine::ForCurrentProcess()->HasSwitch("phishark-local-fixtures");
#else
  return false;
#endif
}

bool CanScanTarget(const GURL& target) {
  if (!target.SchemeIsHTTPOrHTTPS() || target.has_username() || target.has_password()) return false;
  if (UseLocalFixtures()) return target.host() == "127.0.0.1" && target.EffectiveIntPort() == 8765;
  auto host = target.host();
  if (host == "localhost" || host == "metadata" || host == "metadata.google.internal"
      || host == "instance-data" || host == "instance-data.ec2.internal"
      || base::EndsWith(host, ".local") || base::EndsWith(host, ".internal")
      || base::EndsWith(host, ".localhost")) return false;
  net::IPAddress ip;
  if (ip.AssignFromIPLiteral(target.HostNoBrackets())) return ip.IsPubliclyRoutable();
  return true;
}

// Socket evidence belongs to the committed document, not a navigation event.
// Chromium keeps this through history.replaceState/BFCache and deletes it when
// a different document commits, even if the RenderFrameHost is reused.
class PublicDocumentConnection final
    : public content::DocumentUserData<PublicDocumentConnection> {
 public:
  ~PublicDocumentConnection() override = default;
 private:
  friend content::DocumentUserData<PublicDocumentConnection>;
  explicit PublicDocumentConnection(content::RenderFrameHost* frame)
      : content::DocumentUserData<PublicDocumentConnection>(frame) {}
  DOCUMENT_USER_DATA_KEY_DECL();
};
DOCUMENT_USER_DATA_KEY_IMPL(PublicDocumentConnection);

class TabProtection final : public content::WebContentsObserver,
                            public content::WebContentsUserData<TabProtection> {
 public:
  using Completion = base::OnceCallback<void(bool)>;
  ~TabProtection() override = default;
  void Preflight(const GURL& target, int64_t navigation_id, bool same_document, Completion completion) {
    if (settings_version_ == Java_PhiSharkBridge_getSettingsVersion(base::android::AttachCurrentThread())
        && ReuseNavigationScan(generation_, navigation_id_, target_.spec(), navigation_id,
        target.GetWithoutRef().spec(), same_document, committed_generation_ == generation_)) {
      // Join an unfinished URL check; do not resume a duplicate throttle before
      // that check's decision. Never cancel a running deep scan for a hash event.
      if (preflight_pending_) completions_.push_back(std::move(completion));
      else base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
          base::BindOnce(std::move(completion), session_.verdict() == Verdict::kBlocked));
      return;
    }
    Begin(target);
    navigation_id_ = navigation_id;
    preflight_pending_ = true;
    completions_.push_back(std::move(completion));
    if (!CanScanTarget(target)) {
      detail_ = UseLocalFixtures() ? "Local fixture mode is enabled; real websites are not analyzed in this mode."
          : "This address cannot be scanned as a public internet page.";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
          base::BindOnce(&TabProtection::Unavailable, weak_factory_.GetWeakPtr(), false, detail_)); return;
    }
    Start(Profile::kPreflight, std::nullopt);
  }
  void ObserveResponse(content::NavigationHandle* handle) {
    if (handle->GetNavigationId() != navigation_id_
        || handle->GetURL().GetWithoutRef() != target_) return;
    const auto socket = handle->GetSocketAddress();
    const auto& address = socket.address();
    resolved_public_ = address.IsValid() && address.IsPubliclyRoutable();
    if (UseLocalFixtures() && address.IsLoopback() && CanScanTarget(target_)) resolved_public_ = true;
    resolved_generation_ = generation_;
    if (!resolved_public_) {
      awaiting_content_ = false;
      detail_ = "The public internet connection could not be verified; content analysis was not performed.";
      session_.Unavailable(generation_); Update(Profile::kPreflight, "");
    }
  }

 private:
  friend class content::WebContentsUserData<TabProtection>;
  explicit TabProtection(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<TabProtection>(*contents),
        session_(contents->GetBrowserContext()->IsOffTheRecord()) {}
  void Begin(GURL target) {
    screenshot_capture_.reset();
    deep_has_screenshot_ = false;
    ClearBody();
    detail_.clear();
    target_ = target.GetWithoutRef();
    generation_ = session_.Begin(target_.spec());
    awaiting_content_ = false;
    preflight_posts_ = deep_posts_ = preflight_cache_hits_ = deep_cache_hits_ = 0;
    auth_retry_posts_ = capacity_retry_posts_ = 0;
    deep_html_bytes_ = deep_png_bytes_ = deep_http_status_ = deep_net_error_ = 0;
    committed_generation_ = 0;
    capture_started_generation_ = 0;
    resolved_generation_ = 0; resolved_public_ = false;
    replacing_blocked_document_ = false;
    loader_.reset(); completions_.clear(); preflight_pending_ = false;
    weak_factory_.InvalidateWeakPtrs();
    Update(Profile::kPreflight, "");
  }
  void Update(Profile profile, const std::string& score) {
    if (profile == Profile::kDeep) awaiting_content_ = false;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PhiSharkBridge_updateState(env, web_contents(),
        static_cast<int>(session_.verdict()), static_cast<int64_t>(generation_),
        base::android::ConvertUTF8ToJavaString(env, score),
        base::android::ConvertUTF8ToJavaString(env, session_.last_safe_url()),
        profile == Profile::kDeep, base::android::ConvertUTF8ToJavaString(env, detail_),
        static_cast<int>(session_.url_verdict()), profile == Profile::kPreflight && awaiting_content_);
    PublishRequestCounts();
  }
  void PublishRequestCounts() {
    // These are tab-local numbers, never URL/evidence/credential logs.
    Java_PhiSharkBridge_updateRequestCounts(base::android::AttachCurrentThread(),
        web_contents(), static_cast<int64_t>(generation_), preflight_posts_,
        deep_posts_, preflight_cache_hits_, deep_cache_hits_, auth_retry_posts_,
        capacity_retry_posts_, deep_html_bytes_, deep_png_bytes_, deep_http_status_, deep_net_error_);
  }
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame()) return;
    if ((!handle->HasCommitted() || handle->IsErrorPage())
        && handle->GetNavigationId() == navigation_id_ && awaiting_content_) {
      awaiting_content_ = false;
      detail_ = "The page did not finish loading; content analysis was not performed.";
      session_.Unavailable(generation_); Update(Profile::kPreflight, "");
    }
    if (!handle->HasCommitted() || handle->IsErrorPage()
        || !handle->GetURL().SchemeIsHTTPOrHTTPS()) return;
    if (handle->GetNavigationId() == navigation_id_
        && handle->GetURL().GetWithoutRef() == target_
        && resolved_generation_ == generation_ && resolved_public_
        && handle->GetRenderFrameHost()) {
      PublicDocumentConnection::CreateForCurrentDocument(handle->GetRenderFrameHost());
    }
    // Commit paths not covered by a throttle are observed explicitly.
    if (navigation_id_ != handle->GetNavigationId() || target_ != handle->GetURL().GetWithoutRef()) {
      Preflight(handle->GetURL(), handle->GetNavigationId(), handle->IsSameDocument(),
          base::BindOnce([](bool) {}));
    }
    committed_generation_ = generation_;
    if (handle->IsSameDocument() && session_.verdict() != Verdict::kBlocked) Capture();
  }
  void DidStartNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame() || handle->GetURL().SchemeIsHTTPOrHTTPS()) return;
    // Our blank replacement keeps its block dialog. Other internal/data/new-tab
    // navigations invalidate pending scans and allow returning to safety.
    if (replacing_blocked_document_ && handle->GetURL().spec() == "about:blank") return;
    Begin(handle->GetURL()); navigation_id_ = handle->GetNavigationId();
    detail_ = "This browser page is not scanned. Open a website to check it.";
    session_.Unavailable(generation_); Update(Profile::kPreflight, "");
  }
  void DocumentOnLoadCompletedInPrimaryMainFrame() override { Capture(); }
  void WebContentsDestroyed() override {
    screenshot_capture_.reset();
    loader_.reset(); completions_.clear(); ClearBody(); cache_.clear(); session_.Close();
    weak_factory_.InvalidateWeakPtrs();
  }
  void Capture() {
    JNIEnv* env = base::android::AttachCurrentThread();
    // A test-only synthetic loopback session grants no consent for real pages.
    session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(env) || UseLocalFixtures());
    if (awaiting_content_ && !session_.CanCapture()) {
      awaiting_content_ = false;
      detail_ = "Content analysis is not enabled for this session.";
      session_.Unavailable(generation_); Update(Profile::kPreflight, "");
    }
    if (!session_.CanCapture() || session_.verdict() == Verdict::kServiceError
        || !CanScanTarget(target_) || committed_generation_ != generation_
        || preflight_pending_ || loader_
        || web_contents()->GetLastCommittedURL().GetWithoutRef() != target_) return;
    auto* frame = web_contents()->GetPrimaryMainFrame();
    if (!frame || !frame->IsRenderFrameLive()) return;
    if (!PublicDocumentConnection::GetForCurrentDocument(frame)) {
      detail_ = "The public internet connection could not be verified; content analysis was not performed.";
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    if (capture_started_generation_ == generation_) return;
    capture_started_generation_ = generation_;
    awaiting_content_ = false;
    frame->ExecuteJavaScriptInIsolatedWorld(kCaptureScript,
        base::BindOnce(&TabProtection::Captured, weak_factory_.GetWeakPtr(), generation_),
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
  }
  void Captured(uint64_t generation, base::Value value) {
    session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(base::android::AttachCurrentThread())
        || UseLocalFixtures());
    if (generation != generation_ || session_.verdict() == Verdict::kBlocked) return;
    if (!session_.CanCapture()) {
      detail_ = "Content analysis requires normal-mode consent; incognito mode checks URLs only.";
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    const auto* captured = value.GetIfDict();
    const std::string* html = captured ? captured->FindString("html") : nullptr;
    if (!html || html->empty() || html->size() > kMaxHtmlBytes) {
      detail_ = "Page content could not be captured safely or exceeded the size limit.";
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    auto response = captured->Clone();
    response.Set("url", target_.spec()); response.Set("final_url", target_.spec());
    screenshot_capture_ = std::make_unique<ScreenshotCapture>(web_contents(), target_,
        base::BindOnce(&TabProtection::ScreenshotReady, weak_factory_.GetWeakPtr(),
                       generation, std::move(response)));
    screenshot_capture_->Start();
  }
  void ScreenshotReady(uint64_t generation, base::DictValue response,
                       MaskedScreenshot screenshot) {
    screenshot_capture_.reset();
    if (generation != generation_ || session_.verdict() == Verdict::kBlocked) return;
    session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(base::android::AttachCurrentThread())
        || UseLocalFixtures());
    if (!session_.CanCapture() || committed_generation_ != generation_
        || web_contents()->GetLastCommittedURL().GetWithoutRef() != target_) {
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    deep_has_screenshot_ = !screenshot.png_base64.empty()
        && web_contents()->GetVisibility() == content::Visibility::VISIBLE;
    if (UseLocalFixtures()) {
      response.Set("fixture_capture_failure_step", screenshot.failure_step);
      response.Set("fixture_capture_viewport_before", std::move(screenshot.viewport_before));
      response.Set("fixture_capture_viewport_after", std::move(screenshot.viewport_after));
      response.Set("fixture_capture_width", screenshot.width);
      response.Set("fixture_capture_height", screenshot.height);
    }
    if (deep_has_screenshot_) {
      response.Set("screenshot", std::move(screenshot.png_base64));
      // Only explicit synthetic fixtures expose geometry for pixel assertions.
      if (UseLocalFixtures()) {
        response.Set("fixture_mask_regions", std::move(screenshot.regions));
      }
    }
    base::DictValue evidence; evidence.Set("response", std::move(response));
    evidence.Set("capture_coverage", deep_has_screenshot_
        ? "sanitized_html_masked_viewport_png" : "partial_html_no_screenshot");
    Start(Profile::kDeep, std::move(evidence));
  }
  void Start(Profile profile, std::optional<base::DictValue> evidence) {
    auto version = Java_PhiSharkBridge_getSettingsVersion(base::android::AttachCurrentThread());
    if (version != settings_version_) { cache_.clear(); settings_version_ = version; }
    profile_ = profile; attempt_ = 0; auth_retried_ = false; next_retry_ = RetryReason::kNone;
    detail_.clear();
    base::DictValue body; body.Set("target", target_.spec());
    if (evidence) body.Set("web_evidence", std::move(*evidence));
    body_ = base::WriteJson(body).value_or("");
    cache_key_ = (profile == Profile::kDeep ? "deep:" : "preflight:") + crypto::SHA256HashString(body_);
    auto cached = cache_.find(cache_key_);
    if (cached != cache_.end() && cached->second.expires > base::TimeTicks::Now()) {
      if (profile == Profile::kDeep) ++deep_cache_hits_;
      else ++preflight_cache_hits_;
      PublishRequestCounts();
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
          base::BindOnce(&TabProtection::ResultReady, weak_factory_.GetWeakPtr(), cached->second.result));
      return;
    }
    deadline_ = base::TimeTicks::Now() + (profile == Profile::kDeep ? base::Seconds(20) : base::Seconds(10));
    Send();
  }
  void Send() {
    JNIEnv* env = base::android::AttachCurrentThread();
    if (base::TimeTicks::Now() >= deadline_) { Unavailable(false, "The analysis did not finish before its deadline."); return; }
    if (settings_version_ != Java_PhiSharkBridge_getSettingsVersion(env)) { cache_.clear(); Unavailable(true); return; }
    const bool account = !UseLocalFixtures() && Java_PhiSharkBridge_usesAccount(env);
    if (profile_ == Profile::kDeep) {
      session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(env) || UseLocalFixtures());
      if (!session_.CanCapture()) { Unavailable(false); return; }
    }
    std::string base = base::android::ConvertJavaStringToUTF8(env, Java_PhiSharkBridge_getApiBase(env));
    if (UseLocalFixtures()) base = "http://127.0.0.1:8765";
    while (!base.empty() && base.back() == '/') base.pop_back();
    GURL api(base + (profile_ == Profile::kDeep ? "/api/v1/browser/deep" : "/api/v1/browser/preflight"));
    bool endpoint_valid = api.SchemeIs("https") && !api.has_username() && !api.has_password()
        && !api.has_query() && !api.has_ref();
#if BUILDFLAG(PHISHARK_ALLOW_LOOPBACK_TESTING)
    endpoint_valid |= api.SchemeIs("http") && api.host() == "127.0.0.1"
        && !api.has_username() && !api.has_password() && !api.has_query() && !api.has_ref();
#endif
    std::vector<uint8_t> key;
    auto java_key = Java_PhiSharkBridge_getApiKey(env);
    if (!java_key.is_null()) {
      base::android::JavaByteArrayToByteVector(env, java_key, &key);
      std::vector<jbyte> erased(key.size(), 0);
      env->SetByteArrayRegion(java_key.obj(), 0, erased.size(), erased.data());
    }
    if (UseLocalFixtures()) {
      std::fill(key.begin(), key.end(), 0);
      const std::string fixture_key = "fixture-only";
      key.assign(fixture_key.begin(), fixture_key.end());
    }
    if (account && key.empty() && Java_PhiSharkBridge_isAccountRefreshing(env)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(FROM_HERE,
          base::BindOnce(&TabProtection::Send, weak_factory_.GetWeakPtr()), base::Milliseconds(100)); return;
    }
    const bool key_valid = !key.empty() && key.size() <= (account ? 12000u : 4096u)
        && std::all_of(key.begin(), key.end(), [](uint8_t c) { return c >= 33 && c <= 126; });
    if (!endpoint_valid || !key_valid || body_.empty()) {
      std::fill(key.begin(), key.end(), 0);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
          base::BindOnce(&TabProtection::Unavailable, weak_factory_.GetWeakPtr(), true,
              std::string("The account credential or API connection setting is unavailable."))); return;
    }
    auto request = std::make_unique<network::ResourceRequest>(); request->url = api;
    request->method = "POST"; request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    request->redirect_mode = network::mojom::RedirectMode::kError;
    request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
    if (account) request->headers.SetHeader("Authorization", "Bearer " + std::string(key.begin(), key.end()));
    else request->headers.SetHeader("X-API-Key", std::string(key.begin(), key.end()));
    std::fill(key.begin(), key.end(), 0);
    request->headers.SetHeader("Content-Type", "application/json");
    loader_ = network::SimpleURLLoader::Create(std::move(request), kTraffic);
    loader_->SetAllowHttpErrorResults(true); loader_->AttachStringForUpload(body_, "application/json");
    auto remaining = deadline_ - base::TimeTicks::Now();
    if (remaining <= base::TimeDelta()) { Unavailable(false); return; }
    loader_->SetTimeoutDuration(remaining);
    auto* factory = web_contents()->GetBrowserContext()->GetDefaultStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess().get();
    if (profile_ == Profile::kDeep) {
      // Inspect the exact JSON attached for upload, never retain its contents
      // in diagnostics. These numbers describe dispatch, not model coverage.
      deep_html_bytes_ = deep_png_bytes_ = deep_http_status_ = deep_net_error_ = 0;
      auto upload = base::JSONReader::Read(body_, base::JSON_PARSE_RFC);
      const auto* evidence = upload && upload->is_dict()
          ? upload->GetDict().FindDict("web_evidence") : nullptr;
      const auto* response = evidence ? evidence->FindDict("response") : nullptr;
      const auto* html = response ? response->FindString("html") : nullptr;
      const auto* png = response ? response->FindString("screenshot") : nullptr;
      if (html) deep_html_bytes_ = static_cast<int>(html->size());
      if (png && !png->empty() && png->size() % 4 == 0) {
        deep_png_bytes_ = static_cast<int>(png->size() / 4 * 3);
        if (png->back() == '=') --deep_png_bytes_;
        if ((*png)[png->size() - 2] == '=') --deep_png_bytes_;
      }
      ++deep_posts_;
    }
    else ++preflight_posts_;
    if (next_retry_ == RetryReason::kAuth) ++auth_retry_posts_;
    if (next_retry_ == RetryReason::kCapacity) ++capacity_retry_posts_;
    next_retry_ = RetryReason::kNone;
    PublishRequestCounts();
    // Capture, URL checks, document loading and cache hits are not a deep POST.
    // Keep this state through bounded retries until a terminal native update.
    if (profile_ == Profile::kDeep)
      Java_PhiSharkBridge_setDeepPending(env, web_contents(), static_cast<int64_t>(generation_));
    loader_->DownloadToString(factory, base::BindOnce(&TabProtection::ResponseReady,
        weak_factory_.GetWeakPtr()), kMaxResponseBytes);
  }
  void Unavailable(bool service_error, std::string detail = "The analysis could not be completed; the connection or service is temporarily unavailable.") {
    detail_ = std::move(detail);
    loader_.reset(); ClearBody(); session_.Unavailable(generation_, service_error);
    awaiting_content_ = profile_ == Profile::kPreflight && !service_error && ExpectContent();
    ScheduleContentWaitLimit();
    Update(profile_, "");
    CompletePreflight(false);
  }
  void CompletePreflight(bool blocked) {
    if (!preflight_pending_) return;
    preflight_pending_ = false;
    auto callbacks = std::move(completions_);
    completions_.clear();
    auto alive = weak_factory_.GetWeakPtr();
    for (auto& callback : callbacks) {
      std::move(callback).Run(blocked);
      if (!alive) return;
    }
  }
  void ResponseReady(std::optional<std::string> body) {
    if (settings_version_ != Java_PhiSharkBridge_getSettingsVersion(base::android::AttachCurrentThread())) {
      cache_.clear(); Unavailable(true); return;
    }
    int status = 0;
    std::string retry;
    if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers) {
      status = loader_->ResponseInfo()->headers->response_code();
      retry = loader_->ResponseInfo()->headers->GetNormalizedHeader("retry-after").value_or("");
    }
    const int net_error = loader_->NetError(); loader_.reset();
    if (profile_ == Profile::kDeep) {
      deep_http_status_ = status; deep_net_error_ = net_error;
      PublishRequestCounts();
    }
    if (status == 401 && !auth_retried_ && !UseLocalFixtures()
        && Java_PhiSharkBridge_usesAccount(base::android::AttachCurrentThread())
        && base::TimeTicks::Now() < deadline_) {
      auth_retried_ = true;
      next_retry_ = RetryReason::kAuth;
      Java_PhiSharkBridge_refreshAccount(base::android::AttachCurrentThread());
      Send(); return;
    }
    // Authentication/setup failures remain service errors even with a non-JSON
    // gateway response. Capacity retries also cover empty response bodies.
    const bool transient_status = status == 0 || status == 429 || status == 500
        || status == 502 || status == 503 || status == 504;
    if (status >= 400 && !transient_status) {
      Unavailable(true, "The analysis request was not accepted (HTTP " + base::NumberToString(status) + ")."); return;
    }
    if (net_error != net::OK || base::TimeTicks::Now() >= deadline_) {
      Unavailable(false, "The analysis connection did not complete (network code " + base::NumberToString(net_error)
          + ", HTTP " + base::NumberToString(status) + ")."); return;
    }
    auto json = body ? base::JSONReader::Read(*body, base::JSON_PARSE_RFC) : std::nullopt;
    if ((!json || !json->is_dict()) && status != 429) {
      Unavailable(false, "No valid response was received from the analysis service (HTTP " + base::NumberToString(status) + ")."); return;
    }
    const base::DictValue empty;
    const auto& envelope = json && json->is_dict() ? json->GetDict() : empty;
    const auto* code = envelope.FindString("code");
    const bool service_code = code && (*code == "BROWSER_NOT_CONFIGURED" || *code == "QUOTA_EXCEEDED"
        || *code == "LIMIT_EXCEEDED" || *code == "INSUFFICIENT_CREDITS");
    if (status == 429 && !service_code && attempt_++ == 0) {
      double seconds = 0; base::StringToDouble(retry, &seconds);
      auto delay = base::Milliseconds(std::isfinite(seconds) ? std::clamp(seconds * 1000, 0., 2000.) : 0.);
      if (base::TimeTicks::Now() + delay >= deadline_) { Unavailable(false); return; }
      next_retry_ = RetryReason::kCapacity;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(FROM_HERE,
          base::BindOnce(&TabProtection::Send, weak_factory_.GetWeakPtr()), delay); return;
    }
    if (status < 200 || status >= 300) {
      Unavailable(service_code || !transient_status,
          "The analysis service could not complete the request (HTTP " + base::NumberToString(status) + ")."); return;
    }
    const auto* data = envelope.FindDict("data"); if (!data) data = &envelope;
    Result result;
    if (const auto* s = data->FindString("scan_profile")) result.profile = *s;
    if (const auto* s = data->FindString("verdict")) result.verdict = *s;
    if (const auto* s = data->FindString("status")) result.status = *s;
    if (const auto* s = data->FindString("short_circuit_reason")) result.short_circuit_reason = *s;
    result.degraded = data->FindBool("analysis_degraded").value_or(false);
    const auto* risk = data->FindDict("risk_calculation");
    const base::Value* score = risk ? risk->Find("risk_score") : nullptr;
    if (!score || score->is_none()) score = data->Find("risk_score");
    if (score && !score->is_none()) {
      if (score->is_double() || score->is_int()) result.score = score->GetDouble();
      else result.score = std::numeric_limits<double>::quiet_NaN();
    }
    auto verdict = Decide(profile_, result);
    if (verdict == Verdict::kSafe || verdict == Verdict::kWarning || verdict == Verdict::kBlocked) {
      if (cache_.size() >= 128) cache_.erase(cache_.begin());
      cache_[cache_key_] = {result, base::TimeTicks::Now()
          + (profile_ == Profile::kDeep ? base::Minutes(2) : base::Minutes(10))};
    }
    ResultReady(std::move(result));
  }
  void ResultReady(Result result) {
    ClearBody();
    if (settings_version_ != Java_PhiSharkBridge_getSettingsVersion(base::android::AttachCurrentThread())) {
      cache_.clear(); Unavailable(true); return;
    }
    // A failed/unstable screenshot still cannot certify visual coverage.
    detail_.clear();
    if (profile_ == Profile::kDeep && !deep_has_screenshot_
        && Decide(profile_, result) == Verdict::kSafe) {
      result.degraded = true;
      detail_ = "Partial page content was analyzed; screenshot capture was unavailable or could not be masked reliably.";
    } else if (Decide(profile_, result) == Verdict::kUnverified) {
      detail_ = result.degraded ? "The analysis service could not complete some checks."
          : "The analysis result is insufficient for a decision.";
    }
    session_.Apply(generation_, profile_, result);
    awaiting_content_ = profile_ == Profile::kPreflight && ExpectContent();
    ScheduleContentWaitLimit();
    const bool blocked = session_.verdict() == Verdict::kBlocked;
    const std::string score = result.score ? base::NumberToString(*result.score) : "";
    Update(profile_, score);
    // Resuming a deferred navigation can synchronously start another navigation
    // or destroy its tab. Never apply the completed generation to that page.
    const auto completed_generation = generation_;
    auto alive = weak_factory_.GetWeakPtr();
    CompletePreflight(blocked);
    if (!alive || generation_ != completed_generation) return;
    if (blocked && (profile_ == Profile::kDeep || committed_generation_ == generation_)) {
      web_contents()->Stop();
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
          base::BindOnce(&TabProtection::ReplaceBlockedDocument, weak_factory_.GetWeakPtr(), generation_));
    }
    if (profile_ == Profile::kPreflight && !blocked
        && web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) Capture();
  }
  void ReplaceBlockedDocument(uint64_t generation) {
    if (generation != generation_ || session_.verdict() != Verdict::kBlocked) return;
    replacing_blocked_document_ = true;
    content::NavigationController::LoadURLParams params(GURL("about:blank"));
    web_contents()->GetController().LoadURLWithParams(params);
  }
  void ClearBody() {
    std::fill(body_.begin(), body_.end(), '\0');
    body_.clear();
  }
  bool ExpectContent() {
    session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(base::android::AttachCurrentThread())
        || UseLocalFixtures());
    return session_.CanCapture() && session_.verdict() != Verdict::kServiceError
        && CanScanTarget(target_);
  }
  void ScheduleContentWaitLimit() {
    if (!awaiting_content_) return;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(FROM_HERE,
        base::BindOnce(&TabProtection::ContentWaitExpired, weak_factory_.GetWeakPtr(), generation_),
        base::Seconds(30));
  }
  void ContentWaitExpired(uint64_t generation) {
    if (generation != generation_ || !awaiting_content_) return;
    awaiting_content_ = false;
    detail_ = "The page is taking too long to load; content has not been checked yet.";
    session_.Unavailable(generation_); Update(Profile::kPreflight, "");
  }
  NavigationSession session_;
  uint64_t generation_ = 0;
  uint64_t committed_generation_ = 0;
  uint64_t capture_started_generation_ = 0;
  std::unique_ptr<ScreenshotCapture> screenshot_capture_;
  bool deep_has_screenshot_ = false;
  uint64_t resolved_generation_ = 0;
  bool resolved_public_ = false;
  int64_t navigation_id_ = 0;
  bool replacing_blocked_document_ = false;
  GURL target_;
  std::string detail_;
  Profile profile_ = Profile::kPreflight;
  bool preflight_pending_ = false;
  bool awaiting_content_ = false;
  std::vector<Completion> completions_;
  std::string body_, cache_key_;
  int attempt_ = 0;
  bool auth_retried_ = false;
  enum class RetryReason { kNone, kAuth, kCapacity };
  RetryReason next_retry_ = RetryReason::kNone;
  int preflight_posts_ = 0, deep_posts_ = 0;
  int preflight_cache_hits_ = 0, deep_cache_hits_ = 0;
  int auth_retry_posts_ = 0, capacity_retry_posts_ = 0;
  int deep_html_bytes_ = 0, deep_png_bytes_ = 0;
  int deep_http_status_ = 0, deep_net_error_ = 0;
  int64_t settings_version_ = -1;
  base::TimeTicks deadline_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::map<std::string, CachedResult> cache_;
  base::WeakPtrFactory<TabProtection> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
WEB_CONTENTS_USER_DATA_KEY_IMPL(TabProtection);
}  // namespace

void AttachTabProtection(content::WebContents* contents) {
  TabProtection::CreateForWebContents(contents);
}
void NavigationThrottle::MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry) {
  if (registry.GetNavigationHandle().IsInPrimaryMainFrame())
    registry.AddThrottle(std::make_unique<NavigationThrottle>(registry));
}
NavigationThrottle::NavigationThrottle(content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}
NavigationThrottle::~NavigationThrottle() = default;
const char* NavigationThrottle::GetNameForLogging() { return "PhiSharkNavigationThrottle"; }
content::NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillStartRequest() { return Check(); }
content::NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillRedirectRequest() { return Check(); }
content::NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillProcessResponse() {
  auto* protection = TabProtection::FromWebContents(navigation_handle()->GetWebContents());
  if (protection) protection->ObserveResponse(navigation_handle());
  return PROCEED;
}
content::NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillCommitWithoutUrlLoader() { return Check(); }
content::NavigationThrottle::ThrottleCheckResult NavigationThrottle::Check() {
  auto* handle = navigation_handle();
  if (!handle->IsInPrimaryMainFrame() || !handle->GetURL().SchemeIsHTTPOrHTTPS()) return PROCEED;
  TabProtection::CreateForWebContents(handle->GetWebContents());
  auto* protection = TabProtection::FromWebContents(handle->GetWebContents());
  protection->Preflight(handle->GetURL(), handle->GetNavigationId(), handle->IsSameDocument(),
      base::BindOnce(&NavigationThrottle::Complete, weak_factory_.GetWeakPtr()));
  return DEFER;
}
void NavigationThrottle::Complete(bool blocked) {
  if (blocked) CancelDeferredNavigation(ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_CLIENT));
  else Resume();
}
}  // namespace phishark
DEFINE_JNI(PhiSharkBridge)
