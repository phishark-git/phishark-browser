// SPDX-License-Identifier: GPL-3.0-only
package io.phishark.browser.accounttests;
import android.app.Activity;
import android.app.Instrumentation;
import android.os.Bundle;
import io.phishark.browser.security.ApiKeyVault;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.phishark.BrowserAccount;
import org.json.JSONObject;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.io.File;
import java.util.Arrays;

/** Tests actual native account code with synthetic data and no networking. */
public final class AccountProfileInstrumentation extends Instrumentation {
    private String phase;
    private int checks;
    @Override public void onCreate(Bundle args) { super.onCreate(args); phase = args.getString("phase", "verify"); start(); }
    private void require(boolean value) { if (!value) throw new AssertionError(); checks++; }
    private JSONObject data(JSONObject user) throws Exception {
        JSONObject value = new JSONObject().put("access_token", "fixture.fixture.fixture")
                .put("refresh_token", "0".repeat(64)).put("expires_in", 3600)
                .put("client_type", "browser").put("token_type", "Bearer");
        if (user != null) value.put("user", user); return value;
    }
    private void accept(JSONObject user, boolean login) throws Exception {
        Method method = BrowserAccount.class.getDeclaredMethod("accept", JSONObject.class, boolean.class);
        method.setAccessible(true); method.invoke(null, data(user), login);
    }
    private JSONObject stored() throws Exception {
        byte[] bytes = new ApiKeyVault(getTargetContext(), "session").load();
        try { return new JSONObject(new String(bytes, StandardCharsets.UTF_8)); }
        finally { Arrays.fill(bytes, (byte)0); }
    }
    @Override public void onStart() {
        ContextUtils.initialize(getTargetContext()); Bundle result = new Bundle();
        result.putString("phase", phase); result.putInt("pid", android.os.Process.myPid());
        try {
            if ("prepare".equals(phase)) {
                accept(new JSONObject().put("first_name", " Ada ").put("last_name", "İnce")
                        .put("email", "fixture@example.invalid").put("birth_date", "synthetic-private")
                        .put("provider_customer_id", "synthetic-billing"), true);
                require(BrowserAccount.signedIn());
                require("Ada İnce".equals(BrowserAccount.displayName()));
                require("fixture@example.invalid".equals(BrowserAccount.email()));
            } else {
                require(BrowserAccount.signedIn()); // Read the encrypted state from another process.
                require("Ada İnce".equals(BrowserAccount.displayName()));
                require("fixture@example.invalid".equals(BrowserAccount.email()));
                JSONObject profile = stored().getJSONObject("profile");
                require(profile.length() == 3 && profile.has("first_name") && profile.has("last_name") && profile.has("email"));
                require(!profile.has("birth_date") && !profile.has("provider_customer_id"));
                byte[] encrypted = Files.readAllBytes(new File(getTargetContext().getNoBackupFilesDir(), "phishark-session.enc").toPath());
                require(!new String(encrypted, StandardCharsets.ISO_8859_1).contains("fixture@example.invalid"));
                Arrays.fill(encrypted, (byte)0);
                accept(null, false); require("Ada İnce".equals(BrowserAccount.displayName())); // Legacy refresh response.
                accept(new JSONObject().put("first_name", "A\n\u202eB").put("last_name", "X".repeat(321))
                        .put("email", "second@example.invalid"), false);
                require("AB".equals(BrowserAccount.displayName()));
                require("second@example.invalid".equals(BrowserAccount.email()));
                accept(null, true); // A different login must never inherit old identity.
                require(BrowserAccount.displayName().isEmpty() && BrowserAccount.email().isEmpty());
                new ApiKeyVault(getTargetContext(), "pending").save("synthetic-pending".getBytes(StandardCharsets.UTF_8));
                ContextUtils.getAppSharedPreferences().edit().putBoolean("phishark.deep_consent.v1", true).commit();
                Method clear = BrowserAccount.class.getDeclaredMethod("clearSession", String.class);
                clear.setAccessible(true); clear.invoke(null, "test signed out"); // Same native clearing path as Sign out, without a server request.
                require(!BrowserAccount.signedIn());
                require(BrowserAccount.displayName().isEmpty() && BrowserAccount.email().isEmpty());
                require(new ApiKeyVault(getTargetContext(), "session").load() == null);
                require(new ApiKeyVault(getTargetContext(), "pending").load() == null);
                require(!ContextUtils.getAppSharedPreferences().getBoolean("phishark.deep_consent.v1", true));
            }
            result.putString("outcome", "passed"); result.putInt("checks", checks); finish(Activity.RESULT_OK, result);
        } catch (Throwable error) {
            result.putString("outcome", "failed"); result.putInt("checks", checks);
            result.putString("failureType", error.getClass().getSimpleName()); finish(Activity.RESULT_CANCELED, result);
        }
    }
}
