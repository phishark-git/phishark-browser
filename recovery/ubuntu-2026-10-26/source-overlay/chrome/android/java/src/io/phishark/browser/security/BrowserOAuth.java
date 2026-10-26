// SPDX-License-Identifier: GPL-3.0-only
package io.phishark.browser.security;

import java.net.URI;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;

/** Pure native PKCE/callback validation. Never installed as a JavaScript bridge. */
public final class BrowserOAuth {
    public static final String CALLBACK = "io.phishark.browser:/oauth/callback";
    public static String random() {
        byte[] bytes = new byte[32]; new SecureRandom().nextBytes(bytes);
        return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
    }
    public static String challenge(String verifier) throws Exception {
        return Base64.getUrlEncoder().withoutPadding().encodeToString(
                MessageDigest.getInstance("SHA-256").digest(verifier.getBytes(StandardCharsets.US_ASCII)));
    }
    public static Map<String, String> parameters(URI uri) throws Exception {
        Map<String, String> result = new HashMap<>();
        if (uri.getRawQuery() == null) return result;
        for (String part : uri.getRawQuery().split("&")) {
            String[] pair = part.split("=", 2);
            String key = URLDecoder.decode(pair[0], "UTF-8");
            String value = URLDecoder.decode(pair.length == 2 ? pair[1] : "", "UTF-8");
            if (result.put(key, value) != null) throw new IllegalArgumentException("Duplicate parameter");
        }
        return result;
    }
    public static String code(String callback, String state, long created, long now) throws Exception {
        if (callback == null || callback.length() > 8192 || state.isEmpty() || now < created || now - created >= 300000) throw new IllegalArgumentException("Expired flow");
        URI uri = new URI(callback);
        if (!"io.phishark.browser".equals(uri.getScheme()) || uri.getRawAuthority() != null
                || !"/oauth/callback".equals(uri.getRawPath()) || uri.getRawFragment() != null) throw new IllegalArgumentException("Invalid callback");
        Map<String,String> query = parameters(uri);
        if (!MessageDigest.isEqual(state.getBytes(StandardCharsets.UTF_8), query.getOrDefault("state", "").getBytes(StandardCharsets.UTF_8))
                || query.containsKey("error") || query.size() != 2 || query.getOrDefault("code", "").isEmpty()) throw new IllegalArgumentException("Invalid callback state");
        return query.get("code");
    }
    public static void validateAuthURL(String value, String flow, String state) throws Exception {
        URI uri = new URI(value);
        if (!"https".equals(uri.getScheme()) || !"app.phishark.io".equals(uri.getHost()) || uri.getUserInfo() != null
                || uri.getPort() != -1 || !"/browser/connect".equals(uri.getRawPath()) || uri.getRawFragment() != null) throw new IllegalArgumentException("Invalid authorization URL");
        Map<String,String> query = parameters(uri);
        if (query.size() != 2 || !flow.equals(query.get("flow_id")) || !state.equals(query.get("state"))) throw new IllegalArgumentException("Invalid authorization state");
    }
    private BrowserOAuth() {}
}
