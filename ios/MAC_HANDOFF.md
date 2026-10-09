# PhiShark Browser — Mac çalışma ve test rehberi

Hazırlanma: 9 Ekim 2026. Bu belge Mac tarafında insan veya Codex tarafından
izlenebilir. Gerçek test sonuçları henüz bekleniyor.

Güncel hesap/marka görevi: [Mac hesapla ilk giriş rehberi](MAC_ACCOUNT_ONBOARDING.md).
Mac `9064f050` raporunda Fennec baseline ve güvenlik entegrasyonu Xcode 26.6 deney
modunda derlenip açıldı. O çalışmayı koruyarak yeni rehberle devam edin; bu
belgenin aşağıdaki baseline adımları geçmiş başlangıç talimatlarıdır. Ortak dal
PKCE ve ayrı Keychain amaçlarını içerir; native iOS hesap adapter'ı hâlâ gereklidir.

## Hedef ve eşzamanlı çalışma

Mac tarafının ilk hedefi değiştirilmemiş Firefox iOS **Fennec** uygulamasını
derleyip açmak ve bağımsız `PhiSharkSecurity` Swift paketini test etmek.
Windows/WSL tarafında Android başlangıç derlemesi sürerken bu işler yapılabilir.
Android derlemesinin bitmesini beklemek gerekmiyor.

| Çalışma | Sorumlu ortam | Şu anki durum |
| --- | --- | --- |
| Chromium/Cromite ve PhiShark Android | Windows / WSL | Taban ARM64/x64 APK/AAB derlendi. PhiShark x64 APK açıldı; ön kontrol/derin analiz engellemesi ve gizli URL-only kontrolleri geçti. PhiShark ARM64 derleniyor; tam kabul tamamlanmadı |
| Ortak sözleşme ve sunucu/provider değişiklikleri | Windows çalışma dalları | Pushlandı; deployment yapılmadı |
| Swift paketinin gerçek derleme ve testleri | Mac | Bekleniyor |
| Fennec simulator derlemesi ve açılışı | Mac | Bekleniyor |
| iPhone üzerinde açılış ve temel işlev kontrolü | Mac / iPhone | Bekleniyor |
| PhiShark'ın native tarayıcı entegrasyonu | Başlangıç doğrulamasından sonra | Bekleniyor |

JS veya sunucu testlerinin geçmesi Swift, WebKit ya da cihaz testinin geçtiği
anlamına gelmez. Bu ilk Mac doğrulaması tamamlanmış PhiShark iOS uygulaması değildir.

## Kaynaklar ve sürümler

- Repository: <https://github.com/phishark-git/phishark-browser>
- Paylaşılan geliştirme dalı: `codex/browser-mvp`.
- Mac'teki değişiklik ve rapor dalı: `codex/ios-mac-validation`.
- Rehber hazırlanırken test betiklerinin commit'i: `978b3857`.
- Firefox etiketi: `firefox-v157.1`.
- Firefox commit'i: `fbb79fe39c9c53c4da3c4ddb98b14ecf804791e4`.
- Gereken Xcode: **26.5**; Swift paketinin tools version değeri: **6.2**.
- Upstream minimum iOS: **15.0**; Node.js: **22 veya üzeri**.

Sürüm bilgilerinin kaynak dosyası
[`shared/security-contract/upstreams.lock.json`](../shared/security-contract/upstreams.lock.json).
Upstream sürümünü veya kilit dosyasını test hatasını aşmak için değiştirmeyin;
önce gerçek hata ve kurulu sürümler kaydedilmeli.

İşe başlamadan [`AGENTS.md`](../AGENTS.md),
[`security-architecture.md`](../docs/security-architecture.md) ve
[`implementation-status.md`](../docs/implementation-status.md) okunmalı.
Upstream üzerinde ileride kod değiştirilirse onun
[`AGENTS.md`](upstream/AGENTS.md) dosyası da okunmalı.

## 1. Mac hazırlığı

Tam Xcode 26.5 uygulamasını ve Node.js 22+ kurun. Xcode'u bir kez açıp ilk
kurulumu bitirin; iOS Simulator platformunu yükleyin. Xcode Settings > Locations >
Command Line Tools bölümünde tam Xcode uygulaması seçili olmalı.

Terminal'de sürümleri kontrol edin:

```bash
xcodebuild -version
xcode-select -p
swift --version
node --version
npm --version
git --version
```

Xcode sürümü farklıysa bu çıktıyı paylaşın. Betik indirmelere başlamadan sürüm
uyumsuzluğunda durur. Kaynak ve bağımlılık indirmeleri için internet gerekir.
Simulator derlemesi için PhiShark API anahtarı, sunucu erişimi veya Apple signing
anahtarı gerekmiyor.

## 2. Kaynağı al ve Mac dalını oluştur

İlk kurulum:

```bash
mkdir -p ~/Developer
cd ~/Developer
git clone --depth 1 --single-branch --branch codex/browser-mvp \
  https://github.com/phishark-git/phishark-browser.git
cd phishark-browser
git switch -c codex/ios-mac-validation
```

Klasör zaten varsa yeniden clone etmeyin. Önce `git status` ve
`git branch --show-current` ile mevcut çalışmayı kontrol edin. Temiz bir
`codex/browser-mvp` checkout'unda `git pull --ff-only` yapıp Mac dalını oluşturun.
Mac dalı zaten varsa onun üzerinde devam edin; yerel değişiklikleri sıfırlamayın.

Ayrı Mac dalı Android tarafının pushlarıyla çakışmayı önler. Ortak sözleşme,
Android dosyaları veya kilitli upstream sürümleri bu Mac test işi için değişmez.
Yeni ortak değişiklik gerektiğinde iki tarafın test etkisi birlikte değerlendirilir.

## 3. Otomatik doğrulamayı çalıştır

Repository kökünde:

```bash
bash ios/scripts/mac-verify.sh
```

Betik sırayla şunları yapar:

1. Ortamı ve Xcode/Node gereksinimlerini kontrol eder.
2. `npm test` ile ortak sözleşme testlerini çalıştırır.
3. `swift test --package-path ios/security` ile Swift paketini derleyip test eder.
4. Sabitlenmiş Firefox kaynağını ayrı checkout'a alıp commit ve tree hash'ini doğrular.
5. Upstream bootstrap işlemini ve imzasız Fennec simulator derlemesini çalıştırır.

Her testin sonucu ayrı kaydedilir; test hatasında diğer bağımsız kontroller devam
eder ve toplam çıkış kodu başarısız kalır. Ortam kontrolü başarısızsa testler başlamaz.

Upstream bootstrap `.git/hooks` kurduğu için çalışma kopyası `.upstream-cache/`
altında oluşturulur. PhiShark'ın Git hookları ve `ios/upstream/` subtree'si
değiştirilmez. Her çalıştırma yeni upstream checkout oluşturur; önceki kopyalar
silinmez. Bootstrap'ın Nimbus yardımcı betiğini harici `main` dalından alması
upstream sürecinin kalan tekrarlanabilirlik sınırlamasıdır.

## 4. Sonuçları al ve hatayı sınıflandır

```bash
cat .build/mac-verification/summary.txt
cat .build/mac-verification/latest-run.txt
```

Özet sonucu Windows tarafındaki sohbete gönderin. Başarısız adıma göre ilgili
logun son satırlarını ekleyin:

```bash
tail -n 60 "$(cat .build/mac-verification/latest-run.txt)/swift.log"
tail -n 60 "$(cat .build/mac-verification/latest-run.txt)/baseline.log"
```

Ortam kontrolünde durduysa bu loglar oluşmamış olabilir; özet ve sürüm çıktıları
yeterlidir. `.build/` ve `.upstream-cache/` Git tarafından ignore edilir.
Ham signing/provisioning bilgilerini, kişisel API anahtarını veya ortam değişkeni
değerlerini rapora eklemeyin.

| Hata | Sonraki işlem |
| --- | --- |
| Xcode / Node bulunamadı veya sürüm farklı | Kurulu sürümü ve preflight özetini paylaş |
| Swift derleme ya da test hatası | `swift.log` içindeki ilk asıl hatayı ve son satırları paylaş |
| Bootstrap / indirme / hash uyuşmazlığı | Hata satırını kaydet; hash kontrolünü kapatma |
| ModifiedCopy macro doğrulaması | Doğrulanmış projeyi Xcode'da aç; upstream README'deki bu macro için Trust & Enable adımını uygula ve derlemeyi yeniden dene |
| Simulator platformu bulunamadı | Xcode'da platform kurulumunu tamamla; kurulu platformu raporla |
| Signing / gerçek cihaz sorunu | Simulator sonucundan ayrı kaydet; kişisel signing bilgilerini paylaşma |

## 5. Simulator ve iPhone'da açılış

Doğrulanmış proje yolunu açın:

```bash
open "$(cat .build/mac-verification/project-path.txt)"
```

Xcode'da **Fennec** scheme'ini, kurulu bir iPhone simulator hedefini seçip
**Cmd+R** ile çalıştırın. Sonra gerçek iPhone bağlıysa kişisel signing team'inizle
cihaz açılışını deneyin. Signing profilleri ve anahtarları commit edilmez.

Her hedefte gözlenen sonucu kaydedin: uygulama açılışı, normal sayfa açma,
geri/ileri, yeni sekme ve popup, gizli sekme, geçmiş, yer imi, indirme, dosya
yükleme, paylaşım ve izin davranışı. Yapılmayan kontroller `NOT TESTED`,
desteklenmeyenler gerekçesiyle `UNSUPPORTED` olarak yazılmalı.

Yerel sentetik sayfalar için ikinci Terminal'de repository kökünden:

```bash
npm run fixtures
```

Simulator'da `http://127.0.0.1:8765/` açılabilir. Bu aşamada Fennec'in sayfa
davranışını kontrol edin; PhiShark native entegrasyonu henüz olmadığı için
güvenlik API çağrısı veya engelleme beklemeyin. Yerel fixture sunucusu gerçek
model ya da üretim sunucusu çağırmaz. Ayrıntılar
[`local-device-fixtures.md`](../docs/local-device-fixtures.md).

## 6. Mac test raporunu commit et

`docs/mac-validation-report.md` oluşturup aşağıdaki alanları gerçek sonuçlarla doldurun:

```text
Tarih (UTC):
Test edilen browser commit'i:
Firefox upstream commit'i:
Mac mimarisi ve macOS:
Xcode ve Swift:
Node:
Shared tests: PASS / FAIL / NOT TESTED
Swift tests: PASS / FAIL / NOT TESTED
Fennec simulator build: PASS / FAIL / NOT TESTED
Simulator modeli, iOS sürümü ve launch sonucu:
iPhone modeli, iOS sürümü ve launch sonucu:
Temel tarayıcı işlevleri: her kontrolün sonucu
Hata özeti ve kullanılan yerel log referansı:
Yapılan kaynak düzeltmeleri ve yeniden çalıştırılan kontroller:
Kalan engeller:
Native PhiShark entegrasyonu: PENDING
```

Başarısız sonuçlar da teslim edilebilir; test yapılmadan `PASS` yazılmaz.
Raporu ve varsa kapsamı belirli Swift/test düzeltmelerini Mac dalında tutun.
Yalnız rapor değiştiyse:

```bash
git add docs/mac-validation-report.md
git commit -m "docs(ios): record Mac baseline validation results"
git push -u origin codex/ios-mac-validation
```

Windows tarafına dal adını, commit'i, özeti ve kalan engeli bildirin. Ortak dalı
force-push etmeyin; Mac değişiklikleri incelenip ortak dala alınabilir.

## Sonraki iOS aşaması

Hesapla giriş sözleşmesi artık eklenti benzeri PKCE akışıdır:
`POST /api/browser/auth/{start,authorize,token,refresh,logout}`, client `browser`,
scope `browser:scan`, tam callback `io.phishark.browser:/oauth/callback`.
Dashboard dönüş sayfası `/browser/connect`; backend ve dashboard değişiklikleri
henüz üretime alınmadı. Manuel API anahtarı artık ana kurulum deneyimi değildir.

Swift paketindeki `BrowserAccountFlow` PKCE/callback doğrulamasını içerir;
`APIKeyVault(purpose: .session)` ve `.pending` ayrı Keychain kayıtları sağlar.
Mac üzerinde `swift test` ile yeni testleri de çalıştırın. Fennec baseline sonrası
`ASWebAuthenticationSession`, tek seferde tek refresh, atomik token çifti kaydı,
native onay/çıkış ve `Authorization: Bearer` analiz isteklerini bağlayın.
Bu dosyalar iOS girişinin tamamlandığı veya derlendiği anlamına gelmez.
Giriş/çıkışta bekleyen taramaları iptal edin ve önceki hesabın bellek önbelleğini
temizleyin. `/api/v1/browser/*` dışındaki kalıcı scan uçlarına geçmeyin.

İlk açılış, app icon, splash, yeni sekme ve ayarlar PhiShark Browser olacak.
Android'de kullanılan mevcut PhiShark marka görselleri
`android/integration/chromium/chrome/android/java/res_base/drawable-nodpi/`
altında bulunuyor; kullanıcıya görünen Firefox/Mozilla ürün markasını değiştirin,
lisans ve üçüncü taraf bildirimlerini Hakkında bölümünde koruyun.

Değiştirilmemiş Fennec derlemesi ve açılışı doğrulanınca native entegrasyon
noktaları [`native-integration-points.md`](../docs/native-integration-points.md)
üzerinden ele alınır. Gezinme delegate'leri, native networking/cache, Keychain
cihaz davranışı, onay ekranı, kanıt maskeleme, güvenlik ekranları ve marka
değişiklikleri ayrı geliştirme ve kabul işi olarak kalır.

`ios/upstream/` kalıcı entegrasyon kaynak yeridir; `.upstream-cache/` altındaki
derleme kopyasına yapılan değişiklikler teslim edilmiş kaynak sayılmaz.
Ortak sözleşmeyi değiştiren bir ihtiyaç önce Windows tarafıyla koordine edilir.
Üretim deployment'ı ve mağaza yayını bu Mac test görevinin kapsamında değildir.
