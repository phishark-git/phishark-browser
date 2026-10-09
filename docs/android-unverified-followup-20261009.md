# Android kontrol durumu — 9 Ekim 2026

Kullanıcı hesap girişini tamamladıktan sonra `phishark.io/en` sayfasında
“Kontrol edilemedi” gördüğünü bildirdi. Emülatör üzerinde iki ayrı neden bulundu.

## Bulgular ve düzeltmeler

1. Önceki fixture testlerinden `/data/local/tmp/chrome-command-line` içinde
   `--phishark-local-fixtures` kalmıştı. Bu mod gerçek sayfaları bilerek taramaz;
   APK güncellemesi bayrağı temizlemez. Yalnız bu bayrak kaldırıldı ve uygulama
   yeniden başlatıldı; kullanıcı verisi, hesap ve diğer seçenekler korundu.
2. Gerçek sayfa yenilemesinde URL kontrolünün düşük risk sonucu gözlendi, sonra
   durum tekrar belirsiz oldu. `db582dbd` tanılama sürümü panelde sayfanın genel
   internet bağlantısının doğrulanamadığını, içerik analizinin yapılmadığını
   gösterdi. Bu durum oturum hatası değildi: yeniden başlatma sonrası hesap
   panelinde bağlı hesap ve mevcut içerik onayı gözlendi; ayarlar değiştirilmedi.
3. Önceki uygulama, içerik için gereken bağlantı kanıtı eksik olduğunda başarılı
   URL kontrolünü de genel belirsiz durumun altında gizliyordu. `326ae22f`,
   gezinme kimliğine bağlı URL kararını genel/derin karardan ayrı tutar. Genel
   durum belirsizken geçerli düşük riskli URL sonucu varsa “URL düşük risk ·
   Kısmi kontrol” gösterilir. URL uyarısı ayrı korunur. Derin engel, uyarı veya
   hizmet hatası bu metinle örtülmez; eksik içerik güvenli sayılmaz.
4. Kayıtlı oturum diskten yüklenirken panel metni başlangıçtaki “giriş yapın”
   olarak kalabiliyordu. Yüklenen bağlı oturum için metin düzeltildi.

Tanılama yalnız sabit açıklamalar ve sayısal HTTP/ağ kodlarını gösterir. Token,
yanıt gövdesi, URL query'si, parola veya kanıt loglanmaz. Yeni backend API'si,
sunucu bayrağı, üretim deployment'ı veya güvenlik eşik değişikliği yoktur.

## Kapsam ve sınır

Provider/backend sözleşmeleri değişmedi; değişiklik yalnız Android istemci,
native oturum durumu ve C++ → Java JNI görüntüleme ilişkisindedir. Uygulama sırası:
yerel test ayarı → native durum → Java panel → derleme → yerinde APK güncellemesi.
Sunucu deployment sırası yoktur; kurulu emülatör uygulaması güncellenir.

Bağlantı kanıtı eksikse HTML gönderimini engelleyen kontrol korunur. Yeniden
yüklenen/geri getirilen belgelerde socket bilgisi bulunmayabilir; tam kaynağın
neden eksik kaldığı yalnız bu panelden çıkarılamaz. Bu gözlemden bütün sitelerin
veya bütün derin analizlerin çalıştığı sonucu çıkmaz. Tam derin analiz, gerçek
cihazlar, proxy/cache/redirect ve sunucu saklama kabulü hâlâ ayrı testlerdir.

## Doğrulama

- Ortak JS sözleşmesi: 8 test geçti.
- OAuth helper: 19 assertion, marka kontrolleri: 5 test geçti.
- Native C++: 42 karar vektörü ve gezinme invariants geçti. Ek regresyonlar URL
  sonucunun içerik hatasında korunmasını, genel durumun belirsiz kalmasını,
  yalnız URL sonucunun son güvenli sayfa sayılmamasını, yeni gezinmede temizlenmesini,
  eski sonuçların reddini ve derin engelin kaldırılamamasını doğrular.
- Tanılama sürümü `db582dbd`: x64 APK/AAB 5m33s içinde derlendi; `adb install -r`
  başarılı, açılış ve ayrıntılı panel emülatörde gözlendi. Son durum ayrımı
  sürümü `326ae22f`: x64 APK/AAB 5m43s içinde derlendi, yerinde APK kurulumu ve
  açılış başarılı. Aynı gerçek sekmede “URL düşük risk · Kısmi kontrol” görüldü.
  Hesap korundu; fixture runtime bayrağının kapalı olduğu tekrar doğrulandı.

Son APK SHA-256: `2586fd668eedbdf69544ea8f7b863ea3ffa7cbee5bfcdd95623878bc04c7b895`.
AAB SHA-256: `e70c1af82f2c89c1e7926bf543f7545cc0351eee0b619fbbbd592842b167a722`.
Geliştirme imzasıyla x64 emülatör derlemesidir; ARM64 veya mağaza kabulü değildir.

Graphify başlangıç sorgusu `PhiShark unverified BrowserScan BrowserAccount
preflight deep` idi. Browser kod grafiği (520 düğüm / 1.064 ham kenar), component
(908 kenar) ve workspace (13.074 düğüm / 28.765 kenar) yenilendi. Aynı sorgu
110 bağlı düğüm döndürdü; çıktı bütçesi nedeniyle kırpıldı. Toplu grafikte dangling
endpoint yok; component 146 dış AST referansını dışarıda bırakıyor. Belge semantiği,
eski node-ID ve farklı dal snapshot sınırları nedeniyle etki analizi tam değildir.

Üretim sunucusuna SSH veya yeni workflow tetiklemesi yapılmadı. Kullanıcının
mevcut sayfası uygulama içinden kontrol edildi; hiçbir hesap sırrı çıkarılmadı.
