# PhiShark Browser — 9 Ekim üretim yayın incelemesi

Kullanıcı gerekli değişikliklerin üretime alınmasını istedi. Bu dosya resmi
workflow'ların somut kapsamını ve mevcut kanıtı kaydeder. Henüz workflow
tetiklenmedi, PR merge edilmedi, secret veya üretim kaynağı değiştirilmedi.

## Kaynaklar ve sıra

| Sıra | Repo / PR | İncelenen feature commit | Resmi workflow | Hedef / güven |
| --- | --- | --- | --- | --- |
| 1 | g-gatekeeper #2 | `e761e6dc` | `deploy-self-hosted.yml` | VDS primary, yüksek |
| 2 | m-domain-similarity-analysis #2 | `7b7fd2f` | `deploy-cloud-run.yml` | Cloud Run us-central1 primary, SYSTEM_MAP orta güven; canlı hedef doğrulanmadı |
| 2 | m-llm-content-analysis #3 | `395f4a7` | `deploy-cloud-run.yml` | Cloud Run us-central1 primary, orta güven |
| 2 | m-redirection-chain-analysis #2 | `dad0e22` | `deploy-cloud-run.yml` | Cloud Run us-central1 primary, orta güven |
| 2 | m-favicon-analysis #2 | `88fd9b6` | `deploy-cloud-run.yml` | Cloud Run us-central1 primary, orta güven |
| 2 | m-vlm-analysis #2 | `357ebc3` | `deploy-cloud-run.yml` | Cloud Run us-central1 primary, orta güven |
| 3 | m-content-link-analysis #2 | `7461807` | `deploy-self-hosted.yml` | VDS primary, yüksek; Gatekeeper tüketicisi |
| 4 | o-scan-api-orchestrator #23 | `76ad103` | `deploy-self-hosted.yml` | VDS primary, yüksek |
| 5 | b-backend-service #20, ardından #21 | `6df894b`, `725fe1b` | `deploy-self-hosted.yml` | VDS primary, yüksek |
| 6 | w-phishark-dashboard-website #29 | `f0b17e6` | `deploy-hetzner.yml` | VDS primary, yüksek |

Tüm workflow yolları `.github/workflows/` altındadır. Merge öncesi exact head
ve CI tekrar eşleştirilmeli. Main'e merge deploy tetikleyebilir; aynı kaynak
için ayrıca workflow_dispatch gönderip çift yayın yapılmamalı. Backend #21
#20 üzerine yığılıdır; #20 main'e alındıktan sonra #21 base/merge sonucu yeniden
kontrol edilir. Main'deki başka değişiklikler eski feature checkout'u ile
geri alınmamalı. Gatekeeper'a yeni otomatik liste commit'leri bu nedenle
merge edildi; liste içeriği elle değiştirilmedi, Go test/build/vet tekrar geçti.

Beş Cloud Run modülünün son resmi yayınları kaynak tabanlarıyla eşleşiyor.
Bu ve SYSTEM_MAP, hedef seçiminin kanıtıdır; workflow varlığından tek başına
primary sonucu çıkarılmadı. Canlı proje/revizyon/trafik ve sonradan yapılmış
manuel değişiklikler unknown. Başka hedef gösteren kanıt çıkarsa yayın durur.

## Hazırlık, etki ve saklama

- Backend ve orchestrator repo secret listelerinde ve erişilebilir organization
  secret listelerinde `BROWSER_SCAN_INTERNAL_TOKEN` yok. Aynı yeni rastgele
  sunucular arası secret iki repoya güvenli stdin üzerinden konmalı; değeri
  dosyaya, komut çıktısına, dokümana veya mobil uygulamaya yazılmamalı. Kısmi
  secret kurulumunda yayın durmalı; mevcut başka secret döndürülmemeli.
- GitHub değişkenleri backend/orchestrator preflight ve deep'i, orchestrator
  prompt politikasını true gösteriyor. Bunlar canlı konteyner ayarı kanıtı
  değildir. `MOBILE_APP_AUTH_ENABLED` workflow varsayılanı true; değişken yok.
  Mevcut model/provider/key seçimi veya eski istemci bayrakları değiştirilmiyor.
- VDS yapılandırması: Gatekeeper dış port 8081, backend 8000, orchestrator 8080,
  dashboard 3001; uygulama container portları 8080. Content-link yalnız özel
  ağda. Sunucu servisleri `phishark-net` kullanıyor. Yeni host portu eklenmiyor.
- Backend/orchestrator secret volume'ları ve Gatekeeper data/proxy volume'ları
  workflow tarafından yeniden kullanılır. Volume silme, prune, DB migration,
  DNS/firewall/sertifika değişikliği önerilmiyor. Aktif bağlantı/volume/proxy
  durumuna SSH ile bakılmadı; yedeklerin varlığı unknown.
- Normal hesap eşleştirmesi auth-flow/session/refresh/revocation kayıtları ve
  taramalar sayısal kota sayaçları üretir. Bunlar beklenen uygulama yazımlarıdır.
  Browser scan geçmişi/kanıtı/callback'i üretmemeli. Canlı proxy/APM saklama ve
  harici model sağlayıcı koşulları hâlâ ayrıca doğrulanmalıdır.
- Gizlilik provider değişiklikleri mevcut API/puanlama alanlarını korur,
  URL/kanıt taşıyan logları kaldırır. Eski log alanlarını kullanan operasyonel
  aramalar etkilenebilir. Eklenti, web, batch ve mail mevcut yollarını korur.

## Kesinti ve geri dönüş

Backend/orchestrator/dashboard workflow'ları aday container ve sağlık kontrolü
sonra mevcut container'ı `docker stop`/`docker rename` ile kenara alır; yeni
container aynı porta gelir. Kısa kesinti ve devam eden isteklerin başarısız
olması mümkündür. Başarısız aday veya yeni container için `docker rm -f`,
ardından bazı başarısızlık dallarında eski container'ı rename/start ile otomatik
geri getirme vardır. Bu otomatik rollback kapsamı kaynak workflow ile sınırlı;
her hata dalının rollback yaptığı veya sıfır kesinti olduğu iddia edilmez.

Gatekeeper proxy geçişi kullanır; önceki backend'i korur fakat daha eski
etiketli backend/legacy container'ları temizleyen `docker rm -f` adımları vardır.
Content-link workflow'u başarıdan sonra önceki rollback container'ını siler.
Bu yüzden yalnız genel yayın onayı, workspace AGENTS kuralına göre bu silme
adımlarının özel onayı sayılmaz. Mevcut workflow'lar değiştirilmeden bunların
çalışması ayrıca açıkça onaylanmalıdır. Volume/image prune kapsam dışıdır.

Cloud Run workflow'ları yeni image/revizyon yayımlar; bilinen önceki kaynak
commit'leri yukarıdaki provider tabanlarıdır. Tam eski canlı revizyon ve trafik
bölüşümü unknown; ayrıca onaylanmamış gcloud trafik değişimi/manuel rollback
yapılmaz. Hata veya bilinmeyen hedefte sonraki tüketicilerin yayını durur.

## Doğrulama kapsamı

1. Her resmi workflow'un aday ve aktif health/smoke kontrolleri. Gatekeeper ve
   analyzer `/health`, orchestrator `/healthz`; backend workflow'u mevcut
   auth gerektiren yolların 401 durumunu ve iç 9091 `/healthz` kontrolünü kullanır.
   Dashboard mevcut SPA route ve yapılandırılmışsa AASA kontrollerini korur.
2. Yayın sonrasında public `POST /api/browser/auth/start` boş JSON ile 400
   (feature kapalıysa 503), `/api/v1/browser/preflight` ve `/deep` kimliksiz
   401, dashboard `/browser/connect` HTML 200. Yalnız HTTP durumları raporlanır;
   bu kontroller gerçek kullanıcı veya kalıcı scan oluşturmaz.
3. Sentetik kurulum kimliği/PKCE ile bir start isteği 200 ve exact auth_url
   sözleşmesini doğrular; en fazla kısa ömürlü bir auth-flow kaydı yaratır.
   Kod/state/verifier ve tam yanıt loglanmaz. İnsan hesabına giriş kullanıcı
   tarafından tamamlanır; parola/OTP otomasyonu yoktur.
4. Gerçek login, token refresh/logout ve yetkili ephemeral scan/retention
   doğrulaması ayrı kabul sonucudur. Auth başlangıç başarısı bütün tarayıcı
   korumasının güvenilir çalıştığı anlamına gelmez. Canlı scan testi öncesinde
   ilgili provider yayınları başarılı olmalı ve test hesabı/onayı belirlenmeli.

Başlangıç kanıtı: backend #20/#21 ve orchestrator #23 CI başarılı. Dashboard
86 test, serve-config ve build başarılı; #29 mergeable. Providerların önceki
yerel ayrı test/build/vet sonuçları validation-report.md'de; Gatekeeper güncel
listelerle tekrar geçti. Testler üretim runtime durumunun yerine geçmez.

## Özel onay sınırı

Genel üretim talimatı alınmıştır. Tetikleme öncesinde yukarıdaki resmi
workflow'ların container değiştirme/`docker rm -f` temizleme ve kendi otomatik
rollback adımları için özel onay beklenir. Bu sınır workspace `AGENTS.md`:
“Treat destructive or hard-to-reverse operations as requiring specific approval
even when a general deployment was approved.” hükmünden gelir. SSH/VDS
incelemesi bu plana dahil değildir ve ayrıca izin gerektirir.

Graphify: browser/Gatekeeper kod ve component grafikleri, ardından workspace
yenilendi (13.060 düğüm, 28.739 kenar). Başlangıç yayın etki sorgusu tekrarlandı:
734 düğüm, gösterim bütçesi nedeniyle kırpılmış çıktı. Kaynak kontrolü esas
alındı; belge semantiği, farklı dal snapshot'ları ve canlı runtime kapsamı
eksik kalıyor. Bu yenileme üretim doğrulaması değildir.
