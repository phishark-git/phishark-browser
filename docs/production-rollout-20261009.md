# PhiShark Browser — 9 Ekim üretim yayın incelemesi

Kullanıcı gerekli değişikliklerin üretime alınmasını istedi. Bu dosya resmi
workflow'ların somut kapsamını ve mevcut kanıtı kaydeder. Kullanıcı 9 Ekim'de
container değiştirme, mevcut `docker rm -f` temizliği ve workflow içindeki
otomatik rollback dahil somut planı açıkça onayladı. Yayın tamamlandı;
11 resmi workflow ve 5 public smoke kontrolü başarılı. Gerçek çalıştırma
sonuçları aşağıdaki yayın kaydındadır.

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
ve CI tekrar eşleştirilmeli. Orchestrator yalnız `workflow_dispatch` kullanır;
diğer listedeki workflow'lar main push ile başlar. Aynı kaynak
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

- Yayın öncesinde backend ve orchestrator repo secret listelerinde ve erişilebilir
  organization listelerinde `BROWSER_SCAN_INTERNAL_TOKEN` yoktu. Aynı yeni
  kriptografik rastgele sunucular arası secret iki repoya güvenli stdin ile
  kuruldu; iki işlem de başarılı ve yalnız secret adının varlığı doğrulandı.
  Değeri dosyaya, komut çıktısına, dokümana veya mobil uygulamaya yazılmadı.
  Mevcut başka secret döndürülmedi.
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
Bu adımların özel onayı kullanıcıdan alındı; mevcut workflow'lar bu kapsamda
çalıştırılıyor. Volume/image prune kapsam dışıdır.

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

Genel üretim talimatı ve yukarıdaki resmi workflow'ların container değiştirme,
`docker rm -f` temizleme ve kendi otomatik rollback adımları için özel onay
alındı. Bu sınır workspace `AGENTS.md`:
“Treat destructive or hard-to-reverse operations as requiring specific approval
even when a general deployment was approved.” hükmünden gelir. SSH/VDS
incelemesi bu plana dahil değildir ve ayrıca izin gerektirir.

Yayın öncesi Graphify: browser/Gatekeeper kod ve component grafikleri, ardından workspace
yenilendi (13.060 düğüm, 28.739 kenar). Başlangıç yayın etki sorgusu tekrarlandı:
734 düğüm, gösterim bütçesi nedeniyle kırpılmış çıktı. Kaynak kontrolü esas
alındı; belge semantiği, farklı dal snapshot'ları ve canlı runtime kapsamı
eksik kalıyor. Bu yenileme üretim doğrulaması değildir.

## Gerçek yayın sonucu — 9 Ekim 2026

Onaylanan sıra uygulandı: yedi provider → orchestrator → backend #20 → backend #21
→ dashboard. Tüm resmi deployment workflow'ları başarılı tamamlandı. Orchestrator
merge sonrası yalnız bir kez `workflow_dispatch` ile, diğerleri main push ile
başladı. Backend #21, #20 birleştikten sonra main tabanına taşındı; aynı feature
head ve başarılı CI doğrulandı. Workflow dışında SSH, manuel container işlemi,
veritabanı işlemi veya rollback yapılmadı.

| Repo / PR | Üretim merge commit | Resmi workflow run | Sonuç |
| --- | --- | --- | --- |
| g-gatekeeper #2 | `fc9ac145` | [37962689592](https://github.com/phishark-git/g-gatekeeper/actions/runs/37962689592) | success |
| m-domain-similarity-analysis #2 | `3fa8e6c3` | [37963568568](https://github.com/phishark-git/m-domain-similarity-analysis/actions/runs/37963568568) | success |
| m-llm-content-analysis #3 | `ddf743d6` | [37963583193](https://github.com/phishark-git/m-llm-content-analysis/actions/runs/37963583193) | success |
| m-redirection-chain-analysis #2 | `5ce95c2c` | [37963596462](https://github.com/phishark-git/m-redirection-chain-analysis/actions/runs/37963596462) | success |
| m-favicon-analysis #2 | `88c00e9d` | [37963609571](https://github.com/phishark-git/m-favicon-analysis/actions/runs/37963609571) | success |
| m-vlm-analysis #2 | `0e8ef25b` | [37963623306](https://github.com/phishark-git/m-vlm-analysis/actions/runs/37963623306) | success |
| m-content-link-analysis #2 | `1d91515f` | [37963938878](https://github.com/phishark-git/m-content-link-analysis/actions/runs/37963938878) | success |
| o-scan-api-orchestrator #23 | `84ded3fd` | [37964115189](https://github.com/phishark-git/o-scan-api-orchestrator/actions/runs/37964115189) | success |
| b-backend-service #20 | `ff12a696` | [37964594753](https://github.com/phishark-git/b-backend-service/actions/runs/37964594753) | success |
| b-backend-service #21 | `cbcdffcf` | [37964967868](https://github.com/phishark-git/b-backend-service/actions/runs/37964967868) | success |
| w-phishark-dashboard-website #29 | `456dee4f` | [37965268682](https://github.com/phishark-git/w-phishark-dashboard-website/actions/runs/37965268682) | success |

Workflow başarısı, workflow'un uyguladığı kontrollerin kanıtıdır. Gatekeeper
warmup/trafik geçişi, content-link bağımlılık/health, orchestrator ve backend
container health, dashboard deploy/smoke adımları tamamlandı. Cloud Run'da
yayın adımları başarılı; redirection workflow'unun ayrı health adımı da geçti.
Bu kayıt her serviste ayrı uçtan uca tarama yapıldığı iddiası değildir.

### Canlı public smoke — 17:19:52 UTC / 20:19:52 Türkiye

| Kontrol | Sonuç |
| --- | --- |
| Boş JSON ile browser auth start | HTTP 400, beklenen doğrulama hatası |
| Kimliksiz browser preflight | HTTP 401 |
| Kimliksiz browser deep | HTTP 401 |
| Dashboard browser/connect | HTTP 200, HTML |
| Bir sentetik kurulumla S256 PKCE auth start | HTTP 200; success zarfı, flow_id, eşleşen state, tam HTTPS dashboard host/path ve en fazla 300 saniyelik ömür doğrulandı |

Beş kontrol de geçti; redirect izlenmedi, tekrar yapılmadı. Sentetik start yalnız
kısa ömürlü auth-flow oluşturdu. Akış kimliği, state, verifier, auth URL query'si
ve tam yanıt çıktıya/dosyaya yazılmadı. Gerçek kullanıcı girişi veya tarama
başlatılmadı. Auth start yolu artık canlı ve açık; ilk girişteki eksik servis
yolu engeli giderildi. Gerçek uygulama callback'i, token değişimi, yenileme ve
çıkış henüz kullanıcı hesabıyla doğrulanmış değildir.

### Kalan kabul sınırı

Android'de kurulu x64 test sürümünün **PhiShark'a giriş yap** akışı kullanıcıyla
tamamlanmalı. Mac'te [ilk açılış ve hesap dokümanı](../ios/MAC_ACCOUNT_ONBOARDING.md)
uygulanabilir; API başlangıç ve dashboard route hazırlığı yukarıda doğrulandı.
İnsan parolası/OTP otomasyonu yoktur.

Yetkili ephemeral preflight/deep, prompt politikası, gerçek saklama/URL log/APM
davranışı ve harici model sağlayıcı işleme koşulları ayrı kabul testidir.
Kimliksiz 401, analiz bayraklarının çalıştığını kanıtlamaz; bu bayrakların canlı
etkisi bu test yapılana kadar unknown. Gerçek Android ARM64/iPhone testleri,
girişin tüm yaşam döngüsü ve günlük tarayıcı kabulü tamamlanmadığından MVP
tamamlandı veya mağaza sürümü hazır denmiyor. Yeni Android hesabını içeren ARM64
release derlemesi ve iOS native hesap/marka entegrasyonu ayrıca bekliyor.

### Yayın sonrası etki kontrolü

Browser kod grafiği yeniden güncellendi (509 düğüm, 1.046 ham kenar); component
891 kenarla yeniden kuruldu, ardından workspace 13.063 düğüm / 28.748 kenarla
yenilendi. Başlangıç `browser account deployment BROWSER_SCAN_INTERNAL_TOKEN
deploy-self-hosted` sorgusu tekrarlandı: 734 bağlı düğüm, çıktı bütçesi nedeniyle
kırpılmış gösterim. Toplu grafikte dangling endpoint yok; browser component
146 harici AST endpoint'ini dışarıda bırakıyor. Belge semantiği, dört dış kavramın
source_file bilgisi, eski node-ID şeması ve diğer dal snapshot'ları nedeniyle
etki grafiği hâlâ tam kapsamlı değildir. Canlı yayın kanıtı grafikten değil,
yukarıdaki resmi workflow ve public smoke sonuçlarından gelir.
