# Mac doğrulama raporu — 9 Ekim 2026

## Ortam ve kaynak

- Mac: macOS 26.3.2, arm64, Node 22.23.2, Swift 6.3.3.
- Kilitli Xcode: **26.5**. Kullanılan Xcode: **26.6** (Build 17F113), açık `PHISHARK_XCODE_EXPERIMENT=26.6` modu. Sürüm kilidi değiştirilmedi; 26.5 üzerinde doğrulama yapılmadı.
- iOS 26.5 iPhone 17 Pro simülatörü: `C1D88BA4-DDA0-413C-B384-E4A40EF3EA08`.
- Mac dalı: `codex/ios-mac-validation`; başlangıç commit'i `8ac14d8e8a1f1775da0fa7e1c380c3cca17b250c`.
- Firefox etiketi `firefox-v157.1`, kilitli commit `fbb79fe39c9c53c4da3c4ddb98b14ecf804791e4`; checkout commit ve tree hash'i bootstrap öncesinde doğrulandı.

## Sonuçlar

| Kontrol | Sonuç | Kanıt / sınırlama |
| --- | --- | --- |
| `npm test` ortak sözleşme ve HTTP fixture | PASS | 8 test, 0 hata. |
| Varsayılan Xcode sürüm denetimi | PASS | Deney bayrağı olmadan 26.6, çıkış kodu 2 ile reddedildi; `.build/mac-verification/strict-mode-check.log`. |
| `swift test --package-path ios/security` | PASS | Son çalışmada 9 test, 0 hata; transport, kota/hata, gizli mod, eski gezinme, özel önbellek temizleme sırası ve HTML maskeleme. |
| Değiştirilmemiş Fennec, iPhone 17 Pro arm64 imzasız derleme | PASS | Xcode 26.6 `** BUILD SUCCEEDED **`; `.build/mac-verification/targeted-build.log`. İlk genel derleme `ModifiedCopyMacros` güven uyarısında durmuştu; kullanıcı onayıyla sabitlenmiş makro Xcode'da etkinleştirildi. 8 GB RAM nedeniyle iki mimarili genel deneme durduruldu ve `-jobs 2 ARCHS=arm64` ile tamamlandı. |
| Tam `mac-verify.sh` tekrar çalışması | PASS | Ortam, ortak test, Swift test ve değiştirilmemiş Fennec derlemesi geçti; `.build/mac-verification/summary.txt`, çıkış kodu 0. Betik kendi kapsamı gereği UI açılışını `NOT TESTED` raporlar; açılış ayrıca gözlendi. |
| Değiştirilmemiş Fennec simülatörde açılış | PASS | `simctl launch` PID 89447; Firefox kullanım koşulları karşılama ekranı gözlendi. |
| Native paket bağlı entegre Fennec derlemesi | PASS | Son kaynakla Xcode 26.6 `** BUILD SUCCEEDED **`; `.build/ios-integration/integrated-build-isolated-world.log`. İlk entegre deneme SwiftLint işlev uzunluğu kuralında durmuştu; kod ayrıldı ve tekrar derlendi. Son derlemede 0 engelleyici lint hatası vardı. |
| Entegre Fennec simülatörde açılış | PASS | Temiz kurulumda Firefox kullanım koşulları ekranı görüldü; son derleme `simctl launch` PID 97276 ile açılıp tanıtım ekranına ulaştı. |
| Güvenlik kararları, gizli mod, onay, eski sonuç iptali | PASS | Swift paketi testleri ve ortak fixture testleri. Tarayıcı arayüzünde uçtan uca sonuç ayrıca değerlendirilecek. |
| Anahtarın Keychain kullanımı | NOT TESTED | Kod, mevcut `APIKeyVault` ile `ThisDeviceOnly` Keychain sınıfını kullanıyor; simülatör ayarına anahtar kaydetme/okuma henüz gözlenmedi. |
| Native fixture üzerinden güvenli/uyarı/blok, popup/yönlendirme, hata/kota | NOT TESTED | Fixture sunucusu `127.0.0.1:8765` üzerinde 200 yanıtlıyor ve uygulama fixture modunda açıldı; kullanım koşulları ekranı geçilmedi. |
| Canlı yetkili `POST /api/v1/browser/preflight` ve `/deep` | NOT TESTED | Test anahtarı uygulamaya henüz girilmedi. Anahtarsız POST preflight 404 döndü; bu yetkili POST sonucunu kanıtlamaz. Anahtar veya kanıt içeriği loglanmadı. |
| Canlı sunucu kayıtları ve kalıcı kayıt yokluğu | NOT TESTED | Sunucu kayıtlarına erişim ve yetkili istek yok. Gizlilik iddiası doğrulanmış sayılmıyor. |
| Normal gezinme, geri/ileri, sekmeler, gizli sekme, geçmiş, yer imi, indirme, yükleme, paylaşım, izinler | NOT TESTED | İlk açılış ekranı sonrasında UI kabul testi bekliyor. |
| Gerçek iPhone | NOT TESTED | Kapsam dışında; bağlı cihaz yok. |

## Entegrasyon ve bilinen sınırlar

`PhiSharkSecurity` paketi Firefox projesine yerel Swift paketi olarak bağlandı. Kalıcı Firefox değişiklikleri `ios/upstream/` altındadır; ayrı doğrulanmış checkout yalnız derleme alanıdır. Normal HTTP(S) ana çerçeve eylemleri 10 saniyelik preflight sonucunu bekler; kesin blok mevcut gezinmeyi iptal eder. Derin tarama yalnız normal sekmede ve onay açıkken, 20 saniye sınırıyla başlar. Gizli sekmeler URL kontrolüyle sınırlıdır. URLSession geçici yapılandırma, bellek önbelleği, yanıt/kanıt boyutu sınırları ve kalıcı tarama yoluna geri düşmeme sözleşmesi uygulanmıştır. Ekran görüntüsü gönderilmez; WebKit'in ayrı istemci JavaScript bağlamında oluşturulan HTML klonundan form, düzenlenebilir düğüm, gömülü çerçeve, script, stil ve tüm öznitelikler çıkarılır. Üretim kanıtlarının her hassas alanı dışladığı henüz kanıtlanmamıştır.

WebKit popup isteğini kendi başlatır; uygulama aynı isteği elle tekrar yüklemez. Sunucu yönlendirmelerinde hedef URL'nin politika çağrısından önce her zaman yakalanması garanti değildir. Son URL commit/finish sonrasında yeniden kontrol edilir; bu kontrol ara yönlendirme isteğini veya önceki script çalışmasını geri alamaz. Bu nedenle önleyici koruma iddiası yalnız doğrulanmış ana eylem preflight kapsamındadır.

Tarayıcıdaki URL taşıyan bir tanılama satırı kaldırıldı. Test sürümünde Glean yükleme, çökme raporu ve çalışmalar başlangıçta kapatıldı; ilk açılış gizlilik anahtarları kapalı gözlendi. Upstream'in tüm ağ çağrıları ve sunucu tarafı saklama için tam trafik/retansiyon denetimi yapılmadı.
