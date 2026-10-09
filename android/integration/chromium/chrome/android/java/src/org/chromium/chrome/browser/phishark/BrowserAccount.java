// SPDX-License-Identifier: GPL-3.0-only
package org.chromium.chrome.browser.phishark;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import io.phishark.browser.security.ApiKeyVault;
import io.phishark.browser.security.BrowserOAuth;
import org.chromium.base.ContextUtils;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Native account owner. Credentials and PKCE verifier never enter a renderer. */
public final class BrowserAccount {
    public static final String API = "https://api.phishark.io";
    private static final String MODE = "phishark.account.enabled";
    private static final String PENDING = "phishark.account.pending";
    private static final ExecutorService WORK = Executors.newSingleThreadExecutor();
    private static final ScheduledExecutorService DEADLINES = Executors.newSingleThreadScheduledExecutor();
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static JSONObject session;
    private static boolean loaded, refreshing;
    private static long epoch, retryAfter;
    private static Runnable listener;
    private static String status = "PhiShark hesabınıza giriş yapın";

    private static SharedPreferences prefs() { return ContextUtils.getAppSharedPreferences(); }
    private static ApiKeyVault vault(String purpose) { return new ApiKeyVault(ContextUtils.getApplicationContext(), purpose); }
    public static synchronized void setListener(Runnable changed) { listener = changed; }
    private static void notifyChanged() { MAIN.post(() -> { Runnable current; synchronized (BrowserAccount.class) { current = listener; } if (current != null) current.run(); }); }
    private static JSONObject read(String purpose) throws Exception {
        byte[] data = vault(purpose).load();
        if (data == null) return null;
        try { return new JSONObject(new String(data, StandardCharsets.UTF_8)); }
        finally { Arrays.fill(data, (byte)0); }
    }
    private static void save(String purpose, JSONObject data) throws Exception {
        byte[] bytes = data.toString().getBytes(StandardCharsets.UTF_8);
        try { vault(purpose).save(bytes); } finally { Arrays.fill(bytes, (byte)0); }
    }
    private static synchronized void load() {
        if (loaded) return;
        loaded = true;
        try { session = read("session"); } catch (Exception ignored) { status = "Oturum okunamadı; yeniden giriş yapın"; }
    }
    public static synchronized boolean enabled() { return prefs().getBoolean(MODE, false); }
    public static synchronized boolean signedIn() { load(); return enabled() && session != null; }
    public static synchronized String status() { load(); return status; }
    public static synchronized boolean isRefreshing() { return refreshing; }
    public static synchronized byte[] credential() {
        load();
        if (!enabled() || session == null) return null;
        long remaining = session.optLong("expires_at") - System.currentTimeMillis();
        if (remaining < 60000) refresh(false);
        return remaining > 5000 ? session.optString("access_token").getBytes(StandardCharsets.UTF_8) : null;
    }
    public static synchronized void refresh(boolean force) {
        load();
        if (refreshing || session == null || !enabled() || System.currentTimeMillis() < retryAfter) return;
        if (!force && session.optLong("expires_at") - System.currentTimeMillis() >= 60000) return;
        refreshing = true;
        long generation = epoch;
        String token = session.optString("refresh_token");
        if (force) { try { session.put("expires_at", 0); } catch (Exception ignored) {} }
        WORK.execute(() -> {
            try {
                JSONObject response = request("refresh", new JSONObject().put("refresh_token", token), null);
                synchronized (BrowserAccount.class) { if (epoch == generation) accept(response, false); }
            } catch (RequestError error) {
                synchronized (BrowserAccount.class) {
                    if (epoch == generation) {
                        if (error.status == 401 || error.status == 403) clearSession("Oturum sona erdi; yeniden giriş yapın");
                        else { status = "Oturum yenilenemedi; bağlantı kurulunca tekrar denenecek"; retryAfter = System.currentTimeMillis() + 5000; }
                    }
                }
            } catch (Exception ignored) {
                synchronized (BrowserAccount.class) { if (epoch == generation) { status = "Oturum yenilenemedi"; retryAfter = System.currentTimeMillis() + 5000; } }
            } finally {
                synchronized (BrowserAccount.class) { if (epoch == generation) refreshing = false; }
                notifyChanged();
            }
        });
    }
    private static void bumpSettings() {
        prefs().edit().putLong("phishark.settings_version", prefs().getLong("phishark.settings_version", 0) + 1).commit();
    }
    private static void accept(JSONObject data, boolean newLogin) throws Exception {
        String access = data.getString("access_token"), refresh = data.getString("refresh_token");
        long seconds = data.getLong("expires_in");
        if (!access.matches("[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+") || access.length() > 12000 || !refresh.matches("[a-f0-9]{64}")
                || !"browser".equals(data.optString("client_type")) || !"Bearer".equals(data.optString("token_type"))
                || seconds <= 0 || seconds > 86400) throw new IllegalArgumentException("Invalid account response");
        JSONObject next = new JSONObject().put("access_token", access).put("refresh_token", refresh)
                .put("expires_at", System.currentTimeMillis() + seconds * 1000);
        save("session", next); // Persist the replacement pair before making it available.
        session = next; loaded = true; status = "PhiShark hesabınız bağlı"; retryAfter = 0;
        if (newLogin) {
            prefs().edit().putBoolean(MODE, true).putBoolean("phishark.deep_consent.v1", false).commit();
            vault("api-key").clear(); bumpSettings();
        }
    }
    public static void start(Activity owner, Consumer<String> result) {
        final long generation;
        synchronized (BrowserAccount.class) { generation = ++epoch; refreshing = false; prefs().edit().putBoolean(PENDING, false).commit(); }
        WORK.execute(() -> {
            try {
                String verifier = BrowserOAuth.random(), state = BrowserOAuth.random();
                String device = prefs().getString("phishark.installation", "");
                if (device.isEmpty()) { device = java.util.UUID.randomUUID().toString(); prefs().edit().putString("phishark.installation", device).commit(); }
                JSONObject data = request("start", new JSONObject().put("device_id", device).put("device_info", "PhiShark Browser Android")
                        .put("code_challenge", BrowserOAuth.challenge(verifier)).put("code_challenge_method", "S256")
                        .put("state", state).put("redirect_uri", BrowserOAuth.CALLBACK), null);
                String url = data.getString("auth_url");
                BrowserOAuth.validateAuthURL(url, data.getString("flow_id"), state);
                synchronized (BrowserAccount.class) {
                    if (generation != epoch) return;
                    save("pending", new JSONObject().put("state", state).put("verifier", verifier).put("device", device).put("created", System.currentTimeMillis()));
                    if (!prefs().edit().putBoolean(PENDING, true).commit()) throw new IllegalStateException("Cannot persist flow");
                }
                MAIN.post(() -> {
                    synchronized (BrowserAccount.class) { if (generation != epoch) return; }
                    if (owner.isFinishing()) return;
                    try { openExternal(owner, url); result.accept("Giriş sayfasında hesabınızı bağlayın"); }
                    catch (Exception ignored) { result.accept("Giriş için cihazda başka bir tarayıcı bulunamadı"); }
                });
            } catch (RequestError error) {
                // Fixed messages/status only: never expose response bodies, flow
                // identifiers or credentials in the UI or logs.
                final String message;
                if (error.status == 404 || error.status == 405) {
                    message = "PhiShark Browser giriş hizmeti bu sunucuda kullanılamıyor (HTTP " + error.status + "). Hizmetin yayımlanması veya yapılandırılması gerekiyor.";
                } else if (error.status == 429) {
                    message = "Çok fazla giriş denemesi yapıldı. Biraz bekleyip tekrar deneyin (HTTP 429).";
                } else if (error.status >= 500) {
                    message = "PhiShark giriş hizmeti şu anda kullanılamıyor (HTTP " + error.status + "). Daha sonra tekrar deneyin.";
                } else {
                    message = "PhiShark giriş isteği tamamlanamadı (HTTP " + error.status + "). Hizmet yapılandırmasını kontrol edin.";
                }
                MAIN.post(() -> result.accept(message));
            } catch (java.net.SocketTimeoutException ignored) {
                MAIN.post(() -> result.accept("PhiShark giriş hizmeti zamanında yanıt vermedi. Tekrar deneyin."));
            } catch (java.io.IOException ignored) {
                MAIN.post(() -> result.accept("PhiShark giriş hizmetine güvenli bağlantı kurulamadı. İnternet bağlantınızı kontrol edin."));
            } catch (Exception ignored) {
                MAIN.post(() -> result.accept("PhiShark giriş yanıtı doğrulanamadı veya cihazda güvenli saklanamadı. Tekrar deneyin."));
            }
        });
    }
    private static void openExternal(Activity owner, String url) {
        Intent view = new Intent(Intent.ACTION_VIEW, Uri.parse(url)).addCategory(Intent.CATEGORY_BROWSABLE);
        List<Intent> candidates = new ArrayList<>();
        for (ResolveInfo info : owner.getPackageManager().queryIntentActivities(view, 0)) {
            if (info.activityInfo != null && !owner.getPackageName().equals(info.activityInfo.packageName)) {
                candidates.add(new Intent(view).setPackage(info.activityInfo.packageName));
            }
        }
        if (candidates.isEmpty()) throw new IllegalStateException("External browser unavailable");
        Intent chooser = Intent.createChooser(candidates.remove(0), "PhiShark hesabınıza giriş yapın");
        chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS, candidates.toArray(new Intent[0]));
        owner.startActivity(chooser);
    }
    public static void callback(String uri, Consumer<String> result) {
        WORK.execute(() -> {
            try {
                final JSONObject pending;
                final long generation;
                final String code;
                synchronized (BrowserAccount.class) {
                    if (!prefs().getBoolean(PENDING, false)) throw new IllegalStateException("No active flow");
                    pending = read("pending");
                    if (pending == null) throw new IllegalStateException("Missing flow");
                    code = BrowserOAuth.code(uri, pending.getString("state"), pending.getLong("created"), System.currentTimeMillis());
                    if (!prefs().edit().putBoolean(PENDING, false).commit()) throw new IllegalStateException("Cannot consume flow");
                    vault("pending").clear(); generation = ++epoch; refreshing = false;
                }
                JSONObject data = request("token", new JSONObject().put("code", code).put("code_verifier", pending.getString("verifier"))
                        .put("device_id", pending.getString("device")).put("redirect_uri", BrowserOAuth.CALLBACK), null);
                synchronized (BrowserAccount.class) { if (generation != epoch) return; accept(data, true); }
                notifyChanged(); MAIN.post(() -> result.accept(null));
            } catch (Exception ignored) { MAIN.post(() -> result.accept("Giriş tamamlanamadı. PhiShark Browser'dan yeniden deneyin.")); }
        });
    }
    private static void clearSession(String message) {
        session = null; loaded = true; status = message; epoch++; refreshing = false;
        try { vault("session").clear(); vault("pending").clear(); } catch (Exception ignored) {}
        prefs().edit().putBoolean(MODE, false).putBoolean(PENDING, false).putBoolean("phishark.deep_consent.v1", false).commit(); bumpSettings();
    }
    public static synchronized void logout() {
        load(); JSONObject old = session;
        clearSession("PhiShark hesabından çıkış yapıldı"); notifyChanged();
        if (old != null) WORK.execute(() -> {
            try { request("logout", new JSONObject().put("refresh_token", old.getString("refresh_token")), old.getString("access_token")); }
            catch (Exception ignored) { synchronized (BrowserAccount.class) { status = "Bu cihazdan çıkıldı; sunucu oturumunu hesabınızdan da kapatabilirsiniz"; } notifyChanged(); }
        });
    }
    public static synchronized void useDeveloperKey() {
        clearSession("Geliştirici API anahtarı modu"); prefs().edit().putBoolean(MODE, false).commit(); notifyChanged();
    }
    private static final class RequestError extends Exception {
        final int status;
        RequestError(int value) { super("Account request failed"); status = value; }
    }
    private static JSONObject request(String action, JSONObject body, String bearer) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL(API + "/api/browser/auth/" + action).openConnection();
        ScheduledFuture<?> timeout = DEADLINES.schedule(connection::disconnect, 20, TimeUnit.SECONDS);
        byte[] payload = body.toString().getBytes(StandardCharsets.UTF_8);
        try {
            connection.setInstanceFollowRedirects(false); connection.setUseCaches(false);
            connection.setConnectTimeout(10000); connection.setReadTimeout(10000);
            connection.setRequestMethod("POST"); connection.setDoOutput(true);
            connection.setRequestProperty("Content-Type", "application/json");
            connection.setRequestProperty("Cookie", "");
            if (bearer != null) connection.setRequestProperty("Authorization", "Bearer " + bearer);
            connection.setFixedLengthStreamingMode(payload.length);
            try (java.io.OutputStream out = connection.getOutputStream()) { out.write(payload); }
            int status = connection.getResponseCode();
            if (status != 200) throw new RequestError(status);
            try (InputStream input = connection.getInputStream(); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[4096]; int count;
                while ((count = input.read(buffer)) != -1) {
                    if (output.size() + count > 65536) throw new IllegalStateException("Response too large");
                    output.write(buffer, 0, count);
                }
                JSONObject envelope = new JSONObject(output.toString("UTF-8"));
                if (!envelope.optBoolean("success")) throw new RequestError(status);
                return envelope.optJSONObject("data") == null ? new JSONObject() : envelope.getJSONObject("data");
            }
        } finally { Arrays.fill(payload, (byte)0); timeout.cancel(false); connection.disconnect(); }
    }
    private BrowserAccount() {}
}
