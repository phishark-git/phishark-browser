# PhiShark giriş, marka ve devam etme incelemesi

Kaynak incelemesi: 2026-10-09. Bu belge çalışan hesap entegrasyonu veya üretim
doğrulaması değildir. Tarayıcı prototipi hâlâ kişisel API anahtarı kullanır.

**Sonraki uygulama:** Kullanıcının eklenti benzeri giriş talebi üzerine aşağıdaki
eksikler için backend, dashboard ve Android kaynakları eklendi. Güncel kapsam ve
doğrulama [hesap uygulaması](browser-account-implementation.md) belgesindedir.
Bu inceleme önceki durumun kaydıdır; üretim deployment'ı yapılmış sayılmaz.

## ARM emülatöründeki çökme

Windows üzerinde x86_64 Android emülatörü çalışıyor; ARM64 APK çeviri katmanından
geçiyor. Kendi değişikliksiz ARM64 derlememiz, ondan üretilen universal APK ve aynı
sürümün resmî Cromite ARM64 APK'sı JNI istisnası sırasında `FindClassHook` içinde
SIGSEGV veriyor. Resmî ve yerel x64 APK'lar açılıyor. Bu karşılaştırma ARM çevirisi
uyumsuzluğunu destekliyor; fiziksel ARM cihazın çalıştığını kanıtlamıyor.
Hash'ler ve karşılaştırmalar: [emülatör raporu](android-emulator-validation.md).

## Kullanıcı devam edebilir mi?

| İstemci / sonuç | Kaynaktaki davranış |
| --- | --- |
| Playwright preflight 31–85, kesin tehdit yok | Uyarı eklenir; derin inceleme için gezinme devam eder |
| Playwright deep 31–60 | Uyarı eklenir; ayrı onay aracı gerekmeden devam eder |
| Playwright kesin preflight tehdit / preflight ≥86 / deep ≥61 | Engellenir; override aracı yok |
| Playwright geçici analiz hatası / negatif skor | Salt okunur politika; yeniden tarama veya başka sayfaya gezinme |
| Android deep 31–60 | Native uyarıda “Bu gezinme için devam et”; yalnız mevcut gezinme |
| Android kesin tehdit / yüksek risk / prompt politika engeli | Yalnız “Güvenliğe dön”; bypass yok |

Playwright incelemesi hem `main` hem `codex/prompt-injection-evidence` dallarındaki
`src/phishark/guard.js`, `install.js`, `config.js` üzerinden yapıldı. Güvenlik araçları
`phishark_security_status` ve `phishark_rescan`; kullanıcı override aracı kayıtlı değil.
Android uygulaması `PhiSharkBridge.showVerdict()` içinde uyarı devam düğmesini içerir.
Bu inceleme engel politikasını değiştirmez. Playwright'ın hata durumundaki salt okunur
davranışı mobilin doğrulanamadı ile devam etmesinden farklıdır.

## Bir kez giriş yapma önerisi

Önerilen ilk kurulum: **PhiShark'a giriş → analiz için ayrı açık onay → tarayıcı**.
Sonraki açılışlarda native saklanan yenileme belirteciyle oturum sessizce yenilenir.
Parola uygulamada saklanmaz; normal web sitelerindeki oturumlar bundan bağımsızdır.
Çıkış, cihaz oturumunun iptali, hesap kilidi veya yenileme süresinin dolması yeniden
giriş gerektirir. Bu nedenle “sonsuza kadar giriş garantisi” verilmemelidir.

Native giriş için PKCE ve ayrı bir kimlik doğrulama güvenlik alanı kullanılmalı;
tarayıcının kendi normal sekmesinde rastgele bir giriş sayfasına token açılmamalı.
Platform oturum penceresi/dış kullanıcı aracısı yaklaşımı için
[RFC 8252](https://www.rfc-editor.org/rfc/rfc8252) esas alınır. Tarayıcı varsayılan
tarayıcı olduğunda kendisine dönen giriş akışı ayrıca cihazda doğrulanmalıdır.

### Mevcut backend'in sağladıkları

İncelenen backend: `codex/browser-ephemeral-20261009`, commit `219c3fd`.

- `main.go:setupMobileAuthRoutes`: `/api/mobile/auth/start`, `/authorize`, `/token`,
  `/refresh`, `/logout`; `MOBILE_APP_AUTH_ENABLED` kapısı var.
- `services/extension_auth_service.go:NewMobileAuthService`: S256 PKCE,
  kurulum kimliği, tek kullanımlık kod ve tam redirect eşleştirmesi.
- `services/auth_service.go:RefreshAccessToken`: yenileme, cihaz/istemci kapsamının
  korunması, iptal edilmiş belirtecin yeniden kullanımının kontrolü ve hesap kilidi.
- `config/config.go`: mobil yenileme belirtecinin kaynak varsayılanı 30 gün.
  Başarılı yenilemede yeni süre başlar. Uzun süre açılmayan uygulama tekrar giriş
  isteyebilir. Gerçek üretim ayarı **unknown**.
- Dashboard `codex/mobile-native-auth` (`3c1b723`) dalında `/mobile/connect`
  ve mobil callback doğrulaması mevcut. İncelenen canonical dashboard dalında
  bu ekran yok; dalların aynı sürüm olduğu varsayılmamalı.

### Tarayıcı için eksikler

1. Mevcut kayıt yalnız `io.phishark.app:/oauth/callback` kabul ediyor; tarayıcının
   `io.phishark.browser` kimliği için ayrı, tam eşleşen callback/client kaydı gerekli.
   Eski mobil uygulamanın callback'i değiştirilmemeli.
2. `/api/v1/browser/preflight` ve `/deep`, `profileScanMode()` içinde yalnız
   `X-API-Key` doğruluyor. Mobil Bearer token bu uçlara bugün yeterli değil.
   Dar kapsamlı browser oturumu desteği eklenmeli; mevcut API anahtarı yolu korunmalı.
3. Kullanıcı/kurum yetkisi, abonelik kotası ve ücret rezervasyonu browser oturumundan
   çözülmeli. API anahtarı kimliği bekleyen kullanım koduna boş/sahte key ID verilmemeli.
4. Android ve iOS'ta login/callback, güvenli token deposu, tek seferde tek refresh,
   yeniden başlatma ve çıkış akışları henüz bu tarayıcıya bağlanmadı. Oturum değişiminde
   bekleyen taramalar iptal edilmeli ve önceki hesabın bellek önbelleği temizlenmeli.
5. Mevcut refresh kodunda eski belirteç iptali ve yeni belirteç kaydı ayrı işlemler.
   İptal hatasında kod devam edebiliyor; yeni kayıt/yanıt kaybı veya eşzamanlı refresh
   oturumu bozabilir. Uzun süre sorunsuz oturum hedefinden önce atomik rotation ve
   tekrar deneme/yanıt kaybı davranışı test edilip düzeltilmeli.

Oturum kayıtlarının kalıcı saklanması, tarama URL/HTML/kanıtlarının saklanmasıyla
aynı şey değildir. Login entegrasyonu taramaları eski kalıcı scan yollarına taşımamalı.
Gizli mod URL-only ve içerik onayının ayrı olması korunmalı.

### Repository sırası ve uyumluluk

Uygulama sırası: backend auth/oturum ve kota sözleşmesi → dashboard giriş ekranı →
Android → Mac üzerinde iOS. Backend doğrudan sağlayıcı; dashboard ve iki native
istemci doğrudan tüketici; kota/organizasyon servisleri ve orchestrator dolaylı
bağımlılıklar. Orchestrator callback ve mevcut scan sözleşmeleri değişmemeli.
Mevcut mobil uygulama, Safari handoff, extension, Outlook ve dashboard oturumları
yeni browser client türünden dolayı oturum kaybetmemeli.

Deployment sırası: mevcut gizlilik önkoşulu provider'lar → orchestrator → uyumlu
backend → dashboard → mobil sürümler. Üretim iş akışları ayrı onay gerektirir;
bu incelemede üretime erişilmedi ve deployment yapılmadı.

Backend için `go test ./...` ve `go build ./...`, dashboard kendi test/build
komutları, her platform için ayrı cihaz testleri gerekir. Bu turdaki backend
bulguları kaynak incelemesidir; yeni bir auth uygulaması/test başarısı değildir.

## Marka durumu

Mevcut prototipte paket adı, launcher etiketi, fin ikonu, ilk açılış başlığı ve
güvenlik diyalogları PhiShark. Bu değişiklikte cyan/yeşil ikon renkleri, yuvarlatılmış
güvenlik rozeti, risk rengi ve durum değişiminde 220 ms geçiş eklendi. Sistem
animasyonları kapalıysa geçiş çalışmaz; sonsuz yükleme animasyonu kullanılmaz.
İlk açılıştaki işlevsiz Cromite APK güncelleme kutusu ilk gösterimde de gizlenir.

Bu turdaki görsel değişiklikler kaynak düzeyindedir; daha önce gösterilen APK'nın
ekran görüntüleri yeni görsellerin cihaz kanıtı değildir. Devam eden ARM derlemesinin
kaynağı değiştirilmedi. Yeni APK ve görsel kabul kontrolü ayrıca gereklidir.

Doğrulama: `npm test` 8/8 geçti; `apply-integration.py --dry-run` 21 hedefi
doğruladı; `verify-integration.py --output out/phishark_x64_baseline` C++ release,
fixture ve Java API uyumluluğu derlemelerini geçti. Bunlar yeni görünümün cihaz
testi veya backend hesap entegrasyonunun testi yerine geçmez.

Tam marka çalışması bitmedi: nihai ikon, splash, yeni sekme, ayarlar/hakkında,
hata sayfaları ve kalan promosyon/ürün metinleri iki platformda taranmalı. Mevcut
ikon geçici bir native fin çizimidir; dashboard logosunun birebir dönüşümü değildir.
Chromium/Cromite/Mozilla lisans ve üçüncü taraf atıfları korunmalıdır. Genel
sekme/gezinme animasyonları korunur; marka değişikliği normal tarayıcı davranışını
bozmamalıdır. iOS görsel değişiklikleri çalışan Fennec baseline sonrasında yapılır.
