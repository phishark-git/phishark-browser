# PhiShark Browser — Mac üzerinde ilk açılış, hesapla giriş ve marka

9 Ekim 2026. Bu görev mevcut iOS tarayıcı entegrasyonunun devamıdır. Yeni
tarayıcı veya yeni hesap sistemi kurmayın; mevcut PhiShark hesabını eklentideki
eşleştirme mantığıyla bağlayın. Manuel API anahtarı ilk kurulum ekranı değildir.

**Sunucu hazırlığı:** 9 Ekim 2026 20:19 Türkiye saati itibarıyla gerekli resmi
üretim yayınları başarılı. Sentetik S256 PKCE başlangıcı 200, giriş sayfası HTML
200, kimliksiz preflight/deep 401 döndü. Akış/URL/state sözleşmesi doğrulandı;
gerçek hesap girişi, callback, yenileme ve çıkış henüz cihaz kabulü bekliyor.
Commit/run kanıtı: [yayın raporu](../docs/production-rollout-20261009.md).

## Başlangıç durumu ve dal

Mac dalındaki `9064f050` raporu okundu: değiştirilmemiş Fennec ve native güvenlik
paketi bağlanmış Fennec, Xcode 26.6 deney modu ile derlenip simülatörde açılmış.
Kilit hâlâ Xcode 26.5; 26.5 doğrulaması ve gerçek iPhone kabulü yapılmış sayılmaz.
Mevcut Mac değişikliklerini ve bu kanıtı koruyun, baseline işini baştan yapmayın.
İlk açılış sonrası güvenlik/gezinti kabulü ve Keychain cihaz testi hâlâ eksik.

Windows ortak dalı `codex/browser-mvp` artık PKCE helper'ını, ayrı Keychain
amaçlarını ve Android hesap referansını içeriyor. Mac dalı bunlardan önce ayrıldı.
Önce `AGENTS.md`, `docs/security-architecture.md`, `docs/browser-account-implementation.md`,
Mac dalındaki `docs/mac-validation-report.md` ve upstream AGENTS dosyasını okuyun.

Temiz çalışma ağacında, mevcut Mac dalını temel alarak:

```bash
git status --short
git fetch origin
git switch codex/ios-mac-validation
git pull --ff-only origin codex/ios-mac-validation
git switch -c codex/ios-account-onboarding
git merge origin/codex/browser-mvp
```

Dal zaten varsa yeniden oluşturmayın. Yerel değişiklik varsa önce sahipliğini
koruyarak commit edin; reset/force-push yapmayın. Çakışmalarda Mac'in
`BrowserAPIClient`, `BrowserProtectionCoordinator`, HTML temizleme ve WebKit
entegrasyonunu, ortak dalın `BrowserAccountFlow` ve `APIKeyVault.Purpose`
değişikliklerini birlikte koruyun. Tek tarafı topluca seçmeyin.

## İlk açılış deneyimi

### Güncel scanning tercihi ve kanıt sınırı

Kullanıcının son tercihi eski sürekli preflight→deep göstergesini değiştiriyor:
yalnız gerçek deep isteği gönderilmeye başlayınca sabit İngilizce “PhiShark is
scanning this page…” gösterin; preflight, sayfa yükleme, HTML hazırlama ve cache
sonuçlarında göstermeyin. Deep tamamlanana, iptal/hata olana veya sekme değişene
kadar sabit kalsın; retry sırasında yanıp sönmesin. Native API client dispatch
olayına bağlayıp gecikmeli preflight/document/deep aşamalarıyla ayrı test edin.

Android şu anda yalnız temizlenmiş gerçek HTML + URL gönderiyor; screenshot
göndermiyor. Bu değişiklik screenshot hazır olduğu anlamına gelmez. Mac'te de
WKWebView snapshot'ın form/frame/shadow/custom-control maskelemesini cihazda
doğrulamadan screenshot göndermeyin; eksik kanıtla “tam/güvenli” sonucu üretmeyin.
Outgoing links/header metadata ve gerçek redirect zinciri de kendi kanıtıyla
doğrulanmalı. [Güncel audit](../docs/android-deep-only-indicator.md).

### Gatekeeper whitelist düzeltmesi (9 Ekim)

Ortak dalın Swift `ScanResult.trustedPreflight` ve `NavigationSession.canCapture`
değişikliğini de alın. Tamamlanmış, degraded olmayan
`short_circuit_reason: gatekeeper_benign:*` sonucu deep/HTML capture başlatmamalı.
Gatekeeper unknown ise düşük preflight skoru bile içerik analizine devam etmeli;
blacklist kesin engeldir. Whitelist sonucunda bekleme göstergesi bitsin ve
`didFinish`/response/popup/restore callback'leri sonradan deep başlatmasın.
Redirect hedefi kendi preflight'ını almalı; izin yeni generation'a taşınmamalı.
Sayısal analiz yapılmayan whitelist cevabında skor yoksa benign/safe/allowed
kararıyla explicit allow kabul edilir; geçersiz/degraded cevap allow sayılmaz.
Swift paketindeki 58 ortak vektörü ve yeni redirect/allow testini `swift test`
ile çalıştırın. Native adapter ve WebKit entegrasyonunu Mac'te ayrıca doğrulayın:
whitelist 1 preflight/0 deep, unknown 1/1, blacklist 1/0; whitelist → unknown
redirect deep'i atlamamalı. [Sözleşme/kanıt](../docs/gatekeeper-navigation-routing.md).

1. App icon, açılış ekranı ve karşılama başlığı **PhiShark Browser** olsun.
2. Ana eylem **PhiShark'a giriş yap**; ikinci eylem **Şimdilik atla**.
3. Hesap bağlıysa güvenli cihaz oturumunu yükleyin; her açılışta giriş istemeyin.
4. Girişten ayrı, normal modda tam URL'nin query dahil ve temizlenmiş sayfa
   içeriğinin PhiShark'a gönderileceğini açıklayan açık onay gösterin. Harici
   sağlayıcı işleme sınırını gizlemeyin. Gizli mod yalnız URL kontrolüdür.
5. Atlayan kullanıcı tarayıcıyı kullanabilsin; korumayı bağlı/güvenli göstermeyin.
6. Hesap panelinde bağlı durum, çıkış ve hata/yeniden deneme olsun. API anahtarı
   uyumluluğu yalnız Hakkında → geliştirici ayarlarında kalsın.

Marka görselleri `android/integration/chromium/chrome/android/java/res_base/drawable-nodpi/`
altındaki `phishark_mark.png` ve `phishark_wordmark.png`; mevcut resmi PhiShark
dosyalarıdır. Asset catalog varyantlarını bunlardan üretin. Firefox/Mozilla ürün
logolarını app icon, tanıtım, yeni sekme, ayarlar ve boş durumlarda değiştirin.
Lisans/copyright/üçüncü taraf metinleri ve `about` bildirimlerini koruyun. URL
şemaları, sınıf isimleri ve arama sağlayıcı markalarını körlemesine değiştirmeyin.
PhiShark koyu lacivert/turkuaz renkleri kullanın; koruma durum geçişleri kısa ve
Reduce Motion'a uyumlu olsun. VoiceOver ve büyük yazı ile taşmayı kontrol edin.

## Giriş sözleşmesi

API tabanı: `https://api.phishark.io`; dashboard: `https://app.phishark.io`.
Yeni yolların gerçekten yayımlandığına dair
[Windows yayın raporunu](../docs/production-rollout-20261009.md) kontrol edin.
Bu dokümanın veya PR'ın varlığı üretim yayını kanıtı değildir.

| İşlem | Yol | Gövde / davranış |
| --- | --- | --- |
| Başlat | `POST /api/browser/auth/start` | `device_id`, `device_info`, `code_challenge`, `code_challenge_method: "S256"`, `state`, `redirect_uri` |
| Web onayı | `POST /api/browser/auth/authorize` | Dashboard'ın mevcut web oturumu ile `flow_id`, `approve: true`; native uygulama çağırmaz |
| Kod değişimi | `POST /api/browser/auth/token` | `code`, `code_verifier`, aynı `device_id`, aynı `redirect_uri` |
| Yenile | `POST /api/browser/auth/refresh` | `refresh_token` |
| Çıkış | `POST /api/browser/auth/logout` | Access token ile `Authorization: Bearer …`; gövdede `refresh_token` |

İstek ve yanıt JSON'dur. Başarı zarfı `{success: true, data: {...}}`.
Başlatma `data` alanında `flow_id`, `auth_url`, `expires_in` döner. Token
yanıtında `access_token`, `refresh_token`, `expires_in`, `token_type: "Bearer"`,
`client_type: "browser"`, `session_id`, `scopes` bulunur. Scope yalnız
`browser:scan`; tam callback **`io.phishark.browser:/oauth/callback`**.
Eski mobil uygulamanın `io.phishark.app` callback'ini kullanmayın.

`BrowserAccountFlow` ile 32 bayt rastgele verifier/state, base64url ve S256
üretin. Pending flow'u `.pending` Keychain amacıyla saklayın; ömrü en fazla
5 dakika. `auth_url` yalnız HTTPS `app.phishark.io/browser/connect`, eşleşen
`flow_id` ve `state` ile kabul edilir. Native `ASWebAuthenticationSession`
kullanın; oturum nesnesi ve presentation anchor işlem boyunca yaşasın. Mevcut
PhiShark web oturumunun yeniden kullanılmasına izin verin; private browsing
sekmesiyle hesap eşleştirmeyi birbirine karıştırmayın.

Callback'te scheme/path/host/fragment, tekil ve eşleşen state, tekil kod ve
zaman sınırını helper ile doğrulayın. Pending isteği token değişiminden önce
tek seferlik tüketin. İptal, çift callback ve bekleyen farklı girişleri ele alın.
Callback veya tokenı genel tarayıcı sekmesine/JS köprüsüne aktarmayın. Tam
callback URL'si, code, verifier, state veya token loglanmamalı/rapora girmemeli.
Custom URL scheme kayıtlarını ve soğuk açılış/foreground dönüşünü gerçek cihazda
doğrulayın. PKCE nedeniyle başka uygulamanın kodu yakalaması token vermemeli.

## Native oturum ve analiz istemcisi

Bir actor ile tek hesap oturumu ve tek eşzamanlı refresh yönetin.
`APIKeyVault(purpose: .session)` access/refresh/expiry çiftini birlikte atomik
saklasın; `.pending` ayrı olsun. `WhenUnlockedThisDeviceOnly`, senkronizasyon
kapalı; UserDefaults/renderer/localStorage token deposu değildir. Cihaz kilitli
veya Keychain erişilemezken sessizce plaintext depoya düşmeyin.

Mevcut `BrowserAPIClient` API-key constructor'ını bozmayın; native credential
provider ekleyin. Hesap modunda yalnız `Authorization: Bearer …`; geliştirici
modunda yalnız `X-API-Key`. İkisini birlikte göndermeyin. Yalnız
`POST /api/v1/browser/preflight` ve `/api/v1/browser/deep` kullanın; eski kalıcı
scan yollarına fallback yok. URLSession ephemeral/no-cookie/no-cache/no-redirect
sınırları kalsın. Native hesap isteklerinde bütün yanıt okumasını kapsayan
20 saniye ve 64 KiB yanıt sınırı uygulayın.

Access token bitmeden yaklaşık 60 saniye önce yenileyin; 401'de en fazla bir
yenileme/tekrar, mevcut analizin toplam 10/20 saniyesi içinde kalsın. Refresh
işlemlerini birleştirin; ağ hatasında paralel refresh fırtınası üretmeyin.
Backend eski refresh tokenını atomik iptal ederek yenisini üretir. Yeni çift
Keychain'e başarıyla yazılmadan kullanılmamalı. Yanıtın kaybolması yeniden giriş
gerektirebilir; "sonsuz oturum" vaat etmeyin. Kaynak varsayılanları access
15 dakika, yenilenen refresh 30 gündür; sunucu ayarı farklı olabilir.

Giriş/hesap değiştirme/çıkışta kuşak kimliğini artırın, tarama işlerini iptal edip
hesaba bağlı normal/gizli bellek önbelleklerini temizleyin. Eski hesap sonucunu
yeni sayfaya uygulamayın. Çıkış yerelde hemen etkili olsun; sunucu çağrısı
başarısızsa sunucuda iptalin doğrulanmadığını açıkça söyleyin. Sunucu hatasını
başarılı çıkış veya güvenli sayfa sonucu diye göstermeyin.

HTTP 404/405 = hizmet yolu hazır değil; 429 = hız/kota koşulunu ayır;
5xx = hizmet hatası; DNS/TLS/timeout = bağlantı sınıfı; bozuk zarf = doğrulanamadı.
Ham sunucu gövdesini göstermeyin. 401/403 kalıcı oturum hatasında yeniden giriş
isteyin; geçici bağlantı hatasında web gezinmesi doğrulanamadı göstergesiyle
planlanan şekilde devam etsin. Deep 31–60 uyarısında yalnız o gezinmeye devam
edilebilir; kesin tehdit, deep ≥61 ve prompt engelinde bypass yok.

## Mevcut entegrasyon noktaları

Mac `9064f050` kaynakları:

- `ios/security/Sources/PhiSharkSecurity/BrowserAPIClient.swift`
- `ios/security/Sources/PhiSharkSecurity/BrowserProtectionCoordinator.swift`
- `ios/upstream/firefox-ios/Client/Frontend/Browser/BrowserViewController/Extensions/BrowserViewController+WebViewDelegates.swift`
- `ios/upstream/firefox-ios/Client/Frontend/Browser/BrowserViewController/Views/BrowserViewController.swift`
- `ios/upstream/firefox-ios/Client/Frontend/Settings/Main/AppSettingsTableViewController.swift`
- `ios/upstream/firefox-ios/Client/Application/AppLaunchUtil.swift`
- `ios/upstream/firefox-ios/Client/TermsOfServiceManager.swift`

Kalıcı değişiklikler `ios/upstream/` ve `ios/security/` altında olmalı;
`.upstream-cache/` yalnız derleme kopyasıdır. Mevcut tab/geçmiş/yer imi/indirme/
izin/paylaşım işlevlerini ve HTML temizleme/izole JS dünyasını koruyun.
WebKit ara redirect isteğini her zaman gönderilmeden durduramaz; bu sınırı
hesap/marka çalışmasıyla çözülmüş gibi raporlamayın.

## Test, derleme ve teslim

Önce yerel fake transport/URLProtocol ile: PKCE/state/expiry/replay, iptal,
bozuk yanıt, HTTP hata sınıfları, eşzamanlı refresh, 401 tek tekrar, logout ile
refresh yarışı, hesap değişimi ve eski sonuçların reddini test edin. Kimlik
bilgilerinin renderer'a ve loglara girmediğini doğrulayın. Keychain kalıcılığını
uygulamayı kapatıp açarak, ayrı storage amaçlarını ve çıkışı cihazda test edin.

```bash
npm test
swift test --package-path ios/security
# Yalnız önceki Mac raporundaki onaylı Xcode 26.6 deney ortamında:
PHISHARK_XCODE_EXPERIMENT=26.6 PHISHARK_BUILD_JOBS=2 PHISHARK_ARCHS=arm64 \
  bash ios/scripts/integrated-build.sh
```

Xcode 26.5 kuruluysa deney değişkenini vermeyin. Uygun simulator/device
destination'ını mevcut script desteğiyle seçin. Başarılı compile, uygulamanın
açılması ve kullanıcı girişinin tamamlanması ayrı sonuçlardır.

Yukarıdaki yayın raporu giriş yollarının hazır olduğunu doğruluyor. Native
entegrasyon ve yerel testler tamamlanınca kullanıcı kendi hesabıyla denemeli: ilk giriş,
uygulamayı yeniden açma, access expiry/refresh, offline/online, çıkış, tekrar
giriş, normal/gizli sekme ve hesap değişimi. İnsan parolası/OTP'sini otomatik
girmeyin veya loglamayın. Sunucu/veritabanı ayarlarını Mac'ten değiştirmeyin.

`docs/mac-account-validation-report.md` oluşturun: kaynak commit, Xcode/OS,
simulator ve gerçek cihaz ayrımı, test sayıları, build/açılış sonuçları,
kimliksiz ekran görüntüleri, live endpoint hazırlığı ve kalan engeller.
Gerçek hesap/tarayıcı oturumu testini fixture testiyle birleştirmeyin.

```bash
git status --short
# Yalnız bu görevin incelenmiş dosyalarını stage edin.
git commit -m "Add PhiShark iOS account onboarding and branding"
git push -u origin codex/ios-account-onboarding
```

Sonuçta commit/dal adını, rapor yolunu ve kalan işleri Windows tarafına bildirin.
Mağaza yayını, üretim deployment'ı veya Apple hesabı/sertifika değişimi bu Mac
görevinin kapsamında değildir.

## Son tercih: sessiz gezinme ve istek tekrarı

Kullanıcı normal sayfada “düşük risk” veya “kısmi kontrol” sonuç rozeti
istemiyor. Hesap/koruma ayrıntılarını menüye koyun; yalnız risk kararında native
uyarı/engel açın. Belirsiz/hizmet hatası sonuçlarını güvenli olarak değiştirmeyin.
Mevcut 31–60 derin uyarı/devam ve >=61 kesin engel politikasını koruyun.

Android'de aynı belge/aynı canonical URL için History API olaylarının yeni nesil
başlatıp derin analizi tekrarladığı ölçüldü: tek açılışta 1 preflight + 6 deep.
iOS delegelerinde eşdeğer olayı kontrol edin. Normal akış 1 preflight + 1 deep;
beş aynı-URL replaceState/hash olayı bu sayıyı artırmamalı. Aktif isteği koruyun,
preflight bitmeden capture başlatmayın. Değişen path/query, redirect, reload/yeni
belge ve ayarlar/hesap değişikliği ayrı güvenlik bağlamıdır. Gizli mod deep
göndermemeli. 401 yenileme ve tek kapasite-429 tekrarını ölçümde ayrıca belirtin.

Yerel `/pages/duplicate-history` fixture'ı HTML'i değiştirerek beş replaceState
olayı üretir; `/stats` senaryo başına istek sayar. Android testini iOS doğrulaması
olarak kullanmayın. [Ayrıntılı kapsam](../docs/android-request-deduplication.md).

### Aktif analiz göstergesi — güncel tercih

Kullanıcı yalnız analiz sürerken küçük ve rahatsız etmeyen bir gösterge istedi.
“PhiShark kontrol ediyor · Bekleyin” metnini aktif sekmede gösterin; hızlı/cache
yanıtlarında titreşimi önlemek için kısa gecikme kullanın. İş bittiğinde veya
hata/iptalde gizleyin; düşük risk/kısmi kontrol rozeti göstermeyin. Yüzde veya
güvenli sonucu uydurmayın. Gösterge yeni istek başlatmamalı; sekme/gezinme kimliği
ile stale olayları reddetmeli. Bu bir bilgi göstergesidir, yeni etkileşim engeli
değildir. Mevcut risk uyarısı ve kesin engel ekranları korunur.

### İstek sayacı ve analiz aşaması

Gösterge URL kontrolünde “PhiShark adresi kontrol ediyor · Bekleyin”, deep
aşamasında “PhiShark içeriği analiz ediyor · Bekleyin” desin. Koruma paneli
sonucun preflight mı deep mi olduğunu ve o gezinme neslindeki gerçek analiz
POST denemelerini göstersin. URL/deep sayıları, önbellek kullanımı ve 401/kapasite
tekrarları ayrı olsun. Sayaçları yalnız bellekte tutun; URL, içerik, kimlik veya
yanıt loglamayın. Hesap yenileme ve sayfa kaynak istekleri bu sayaca dahil değil.
Yeni nesilde sıfırlayın; aynı belge/URL olaylarında koruyun. Redirect zinciri
ve sekmeler toplamı gibi sunmayın. Android'in sonucu iOS doğrulaması değildir.
[Kapsam ve doğrulama](../docs/android-request-observability.md).

### Güncel tercih: kesintisiz gösterge ve İngilizce arayüz

PhiShark arayüzünü tamamen İngilizce yapın; Türkçe sabit metin bırakmayın.
Metinleri platform string kaynaklarına taşıyın; İngilizce default yeterli.
Tarayıcının dil ayarlarını veya kullanıcının web içeriğini değiştirmeyin.
URL kontrolü → belge yükleme → deep arasında göstergeyi kapatıp yeniden açmayın.
Tek sabit metin: “PhiShark is checking this page…”; aşama ayrıntısı panelde kalsın.
Redirect nesil değişiminde aktif sekmede gösterge görünüyorsa koruyun; terminal
sonuç, yükleme hata/iptali, dahili sayfa ve sekme değişiminde temizleyin.
Belge beklemesini açık durum olarak modelleyin; uzun yüklemeyi sonsuz “scanning”
olarak göstermeyin. Android belge beklemesini 30 saniyede unverified yapar;
sonradan belge yüklenirse içerik analizi mevcut kuralla ayrıca başlayabilir.
Gizli/onaysız oturumda deep beklemesi oluşturmayın. İstek sayaçlarını karşılaştırın;
UI değişikliği yeni analiz isteği çıkarmamalı. Mac cihaz doğrulaması ayrıdır.
