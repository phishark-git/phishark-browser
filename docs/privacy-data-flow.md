# Privacy and data flow

Consent text for normal mode:

> PhiShark koruması, ziyaret ettiğiniz tam web adresini query parametreleriyle birlikte PhiShark API'sine gönderir. Normal modda sayfa içeriği ve hassas giriş alanları maskelenmiş ekran görüntüsü derin analiz için gönderilebilir. Cookie, yetkilendirme başlıkları ve form değerleri gönderilmez. Analiz, harici model sağlayıcıları tarafından işlenebilir. Gizli modda yalnız URL kontrolü yapılır. PhiShark Browser taramaları kalıcı PhiShark tarama geçmişine ve kanıt depolarına kaydedilmez.

This is the required release behavior. Native masking/capture and live retention verification are still pending; do not present the release privacy claim until these pass.

Backend persists numeric usage counters and existing API-key/account metadata. It does not create browser scan documents. Orchestrator handles target/evidence/results in bounded transient execution, suppresses persistence/callbacks and clears state. Unit probes verify persistence method suppression; they do not inspect a live Firestore/GCS deployment. Browser URLs must remain in POST bodies, never API query parameters. Proxy/APM/body capture and downstream module logs are release gates; their runtime settings are **unknown**.

Deep analysis can call phishing modules and prompt analysis, with Google Gemini or other configured providers. Gemini Developer API and Vertex AI Gemini API/express are distinct surfaces; the workspace inventory is the source for each module. Provider retention, regional processing, abuse monitoring and contractual settings are outside the browser's own storage controls. Record actual provider/API/model/configuration and the applicable terms before claiming non-retention. No new LLM provider, model or shared model key is introduced by this change.

The [external-provider terms review](external-provider-terms.md) records the
currently published terms separately from unverified runtime/account settings.
PhiShark's no-history behavior does not imply that every external provider has
zero retention. Seven provider logging patches are mandatory rollout
prerequisites; proxy/APM settings and external terms remain independent gates.

Private mode still transmits the full URL (query can contain personal data). It does not mean anonymous network access or invisibility to the visited site/API provider. Local private-session history, evidence and verdict caches must disappear on closure. Account access/refresh tokens, pending PKCE state and the optional developer API key use separate secure-storage purposes. Android uses AndroidKeyStore with ciphertext in the no-backup directory; the iOS helper uses non-synchronizing WhenUnlockedThisDeviceOnly Keychain items. The iOS account adapter remains a Mac integration task. Logout disables the local account immediately and requests server revocation; offline logout cannot confirm server revocation. Backend account session, refresh-token and hashed session-revocation records persist independently of ephemeral scans and contain no visited URLs or page evidence. The remembered session renews subject to expiry, revocation and account policy; it is not an unlimited session guarantee.

Before release, capture fixtures containing email/password/textarea/select/editable values, authorization/cookie values, sensitive URL queries, cross-origin frames and shadow inputs. Inspect captured payloads and screenshots locally without logging their contents. Verify no records/artifacts/callbacks and no URL logs across application, proxy and every participating module. External-provider terms remain separately disclosed even if PhiShark persistence is disabled.
