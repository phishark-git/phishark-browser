// SPDX-License-Identifier: GPL-3.0-only
#include "chrome/browser/phishark/navigation_throttle.h"
#include <algorithm>
#include <cmath>
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
  for (const element of copy.querySelectorAll('input,textarea,select,button,[contenteditable]')) {
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
  return html.length <= 1048576 ? html : null;
})())JS";

constexpr net::NetworkTrafficAnnotationTag kTraffic =
    net::DefineNetworkTrafficAnnotation("phishark_ephemeral_browser_analysis", R"(
      semantics {
        sender: "PhiShark Browser protection"
        description: "Checks a document URL and optionally consented sanitized page evidence."
        trigger: "Main document navigation with a user-configured PhiShark API key."
        data: "Full URL, personal API authentication header, optional sanitized HTML."
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
#if BUILDFLAG(PHISHARK_ALLOW_LOOPBACK_TESTING)
  if (target.host() == "127.0.0.1") return true;
#endif
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
      detail_ = UseLocalFixtures() ? "Yerel test modu açık; gerçek siteler bu modda analiz edilmez."
          : "Bu adres genel internet sayfası olarak taranamıyor.";
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
      detail_ = "Sayfanın genel internet bağlantısı doğrulanamadı; içerik analizi yapılmadı.";
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
    ClearBody();
    detail_.clear();
    target_ = target.GetWithoutRef();
    generation_ = session_.Begin(target_.spec());
    committed_generation_ = 0;
    capture_started_generation_ = 0;
    resolved_generation_ = 0; resolved_public_ = false;
    replacing_blocked_document_ = false;
    loader_.reset(); completions_.clear(); preflight_pending_ = false;
    weak_factory_.InvalidateWeakPtrs();
    Update(Profile::kPreflight, "");
  }
  void Update(Profile profile, const std::string& score) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_PhiSharkBridge_updateState(env, web_contents(),
        static_cast<int>(session_.verdict()), static_cast<int64_t>(generation_),
        base::android::ConvertUTF8ToJavaString(env, score),
        base::android::ConvertUTF8ToJavaString(env, session_.last_safe_url()),
        profile == Profile::kDeep, base::android::ConvertUTF8ToJavaString(env, detail_),
        static_cast<int>(session_.url_verdict()));
  }
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted()
        || handle->IsErrorPage() || !handle->GetURL().SchemeIsHTTPOrHTTPS()) return;
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
    detail_ = "Bu tarayıcı sayfası taranmaz. Kontrol için bir web sitesi açın.";
    session_.Unavailable(generation_); Update(Profile::kPreflight, "");
  }
  void DocumentOnLoadCompletedInPrimaryMainFrame() override { Capture(); }
  void WebContentsDestroyed() override {
    loader_.reset(); completions_.clear(); ClearBody(); cache_.clear(); session_.Close();
    weak_factory_.InvalidateWeakPtrs();
  }
  void Capture() {
    JNIEnv* env = base::android::AttachCurrentThread();
    // A test-only synthetic loopback session grants no consent for real pages.
    session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(env) || UseLocalFixtures());
    if (!session_.CanCapture() || session_.verdict() == Verdict::kServiceError
        || !CanScanTarget(target_) || committed_generation_ != generation_
        || preflight_pending_ || loader_
        || web_contents()->GetLastCommittedURL().GetWithoutRef() != target_) return;
    auto* frame = web_contents()->GetPrimaryMainFrame();
    if (!frame || !frame->IsRenderFrameLive()) return;
    if (!PublicDocumentConnection::GetForCurrentDocument(frame)) {
      detail_ = "Sayfanın genel internet bağlantısı doğrulanamadı; içerik analizi yapılmadı.";
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    if (capture_started_generation_ == generation_) return;
    capture_started_generation_ = generation_;
    Java_PhiSharkBridge_setDeepPending(env, web_contents(), static_cast<int64_t>(generation_));
    frame->ExecuteJavaScriptInIsolatedWorld(kCaptureScript,
        base::BindOnce(&TabProtection::Captured, weak_factory_.GetWeakPtr(), generation_),
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
  }
  void Captured(uint64_t generation, base::Value value) {
    session_.SetConsent(Java_PhiSharkBridge_hasDeepConsent(base::android::AttachCurrentThread())
        || UseLocalFixtures());
    if (generation != generation_ || session_.verdict() == Verdict::kBlocked) return;
    if (!session_.CanCapture()) {
      detail_ = "İçerik analizi için normal mod onayı gerekiyor; gizli mod yalnız URL kontrolüdür.";
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    const std::string* html = value.GetIfString();
    if (!html || html->empty() || html->size() > kMaxHtmlBytes) {
      detail_ = "Sayfa içeriği güvenli biçimde alınamadı veya boyut sınırını aştı.";
      session_.Unavailable(generation_); Update(Profile::kDeep, ""); return;
    }
    base::DictValue response; response.Set("html", *html); response.Set("url", target_.spec());
    base::DictValue evidence; evidence.Set("response", std::move(response));
    // No screenshot is transmitted until native pixel masking is verified.
    evidence.Set("capture_coverage", "partial_html_no_screenshot");
    Start(Profile::kDeep, std::move(evidence));
  }
  void Start(Profile profile, std::optional<base::DictValue> evidence) {
    auto version = Java_PhiSharkBridge_getSettingsVersion(base::android::AttachCurrentThread());
    if (version != settings_version_) { cache_.clear(); settings_version_ = version; }
    profile_ = profile; attempt_ = 0; auth_retried_ = false;
    detail_.clear();
    base::DictValue body; body.Set("target", target_.spec());
    if (evidence) body.Set("web_evidence", std::move(*evidence));
    body_ = base::WriteJson(body).value_or("");
    cache_key_ = (profile == Profile::kDeep ? "deep:" : "preflight:") + crypto::SHA256HashString(body_);
    auto cached = cache_.find(cache_key_);
    if (cached != cache_.end() && cached->second.expires > base::TimeTicks::Now()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
          base::BindOnce(&TabProtection::ResultReady, weak_factory_.GetWeakPtr(), cached->second.result));
      return;
    }
    deadline_ = base::TimeTicks::Now() + (profile == Profile::kDeep ? base::Seconds(20) : base::Seconds(10));
    Send();
  }
  void Send() {
    JNIEnv* env = base::android::AttachCurrentThread();
    if (base::TimeTicks::Now() >= deadline_) { Unavailable(false, "Analiz süre sınırında tamamlanamadı."); return; }
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
              std::string("Hesap kimliği veya API bağlantı ayarı kullanılamıyor."))); return;
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
    loader_->DownloadToString(factory, base::BindOnce(&TabProtection::ResponseReady,
        weak_factory_.GetWeakPtr()), kMaxResponseBytes);
  }
  void Unavailable(bool service_error, std::string detail = "Analiz tamamlanamadı; bağlantı veya hizmet geçici olarak kullanılamıyor.") {
    detail_ = std::move(detail);
    loader_.reset(); ClearBody(); session_.Unavailable(generation_, service_error); Update(profile_, "");
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
    if (status == 401 && !auth_retried_ && !UseLocalFixtures()
        && Java_PhiSharkBridge_usesAccount(base::android::AttachCurrentThread())
        && base::TimeTicks::Now() < deadline_) {
      auth_retried_ = true;
      Java_PhiSharkBridge_refreshAccount(base::android::AttachCurrentThread());
      Send(); return;
    }
    // Authentication/setup failures remain service errors even with a non-JSON
    // gateway response. Capacity retries also cover empty response bodies.
    const bool transient_status = status == 0 || status == 429 || status == 500
        || status == 502 || status == 503 || status == 504;
    if (status >= 400 && !transient_status) {
      Unavailable(true, "Analiz isteği kabul edilmedi (HTTP " + base::NumberToString(status) + ")."); return;
    }
    if (net_error != net::OK || base::TimeTicks::Now() >= deadline_) {
      Unavailable(false, "Analiz bağlantısı tamamlanamadı (ağ kodu " + base::NumberToString(net_error)
          + ", HTTP " + base::NumberToString(status) + ")."); return;
    }
    auto json = body ? base::JSONReader::Read(*body, base::JSON_PARSE_RFC) : std::nullopt;
    if ((!json || !json->is_dict()) && status != 429) {
      Unavailable(false, "Analiz hizmetinden geçerli yanıt alınamadı (HTTP " + base::NumberToString(status) + ")."); return;
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
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(FROM_HERE,
          base::BindOnce(&TabProtection::Send, weak_factory_.GetWeakPtr()), delay); return;
    }
    if (status < 200 || status >= 300) {
      Unavailable(service_code || !transient_status,
          "Analiz hizmeti isteği tamamlayamadı (HTTP " + base::NumberToString(status) + ")."); return;
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
    if (score && (score->is_double() || score->is_int())) result.score = score->GetDouble();
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
    // Partial HTML cannot certify capture/visual privacy. Do not label it safe.
    detail_.clear();
    if (profile_ == Profile::kDeep && Decide(profile_, result) == Verdict::kSafe) {
      result.degraded = true;
      detail_ = "Kısmi sayfa içeriği analiz edildi; ekran görüntüsü doğrulanmadığı için tam güvenlik sonucu verilemiyor.";
    } else if (Decide(profile_, result) == Verdict::kUnverified) {
      detail_ = result.degraded ? "Analiz hizmeti bazı kontrolleri tamamlayamadı."
          : "Analiz sonucu karar vermek için yeterli değil.";
    }
    session_.Apply(generation_, profile_, result);
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
  NavigationSession session_;
  uint64_t generation_ = 0;
  uint64_t committed_generation_ = 0;
  uint64_t capture_started_generation_ = 0;
  uint64_t resolved_generation_ = 0;
  bool resolved_public_ = false;
  int64_t navigation_id_ = 0;
  bool replacing_blocked_document_ = false;
  GURL target_;
  std::string detail_;
  Profile profile_ = Profile::kPreflight;
  bool preflight_pending_ = false;
  std::vector<Completion> completions_;
  std::string body_, cache_key_;
  int attempt_ = 0;
  bool auth_retried_ = false;
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
