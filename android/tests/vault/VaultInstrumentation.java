// SPDX-License-Identifier: GPL-3.0-only
package io.phishark.browser.vaulttests;

import android.app.Activity;
import android.app.Instrumentation;
import android.os.Bundle;
import io.phishark.browser.security.ApiKeyVault;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.KeyStore;
import java.util.Arrays;

/** Test-only app, with no networking and no access to the browser app's UID. */
public final class VaultInstrumentation extends Instrumentation {
    private static final String ALIAS = "io.phishark.browser.personal-api-key.v1";
    private static final byte[] SYNTHETIC = "fixture-only-vault-probe".getBytes(StandardCharsets.UTF_8);
    private String phase;
    private int checks;

    @Override public void onCreate(Bundle arguments) {
        super.onCreate(arguments);
        phase = arguments == null ? "verify" : arguments.getString("phase", "verify");
        start();
    }

    private void require(boolean condition) {
        if (!condition) throw new AssertionError("Vault invariant failed at check " + (checks + 1));
        checks++;
    }

    private interface Operation { void run() throws Exception; }
    private void mustReject(Operation operation) throws Exception {
        boolean rejected = false;
        try { operation.run(); } catch (Exception expected) { rejected = true; }
        require(rejected);
    }

    @Override public void onStart() {
        Bundle result = new Bundle();
        result.putString("phase", phase);
        result.putInt("pid", android.os.Process.myPid());
        ApiKeyVault vault = new ApiKeyVault(getTargetContext());
        try {
            if ("prepare".equals(phase)) {
                vault.clear(); vault.save(SYNTHETIC);
                new ApiKeyVault(getTargetContext(), "session").save(SYNTHETIC);
                new ApiKeyVault(getTargetContext(), "pending").save(SYNTHETIC);
                require(Arrays.equals(vault.load(), SYNTHETIC));
                // Leave only synthetic encrypted data for a separate process to read.
            } else {
                // First assertion verifies persistence across instrumentation processes.
                require(Arrays.equals(vault.load(), SYNTHETIC));
                File file = new File(getTargetContext().getNoBackupFilesDir(), "phishark-api-key.enc");
                byte[] first = Files.readAllBytes(file.toPath());
                require(!new String(first, StandardCharsets.ISO_8859_1).contains("fixture-only-vault-probe"));
                require(first[0] == 1 && first[1] == 12);
                require(Arrays.equals(new ApiKeyVault(getTargetContext()).load(), SYNTHETIC));
                vault.save(SYNTHETIC);
                byte[] second = Files.readAllBytes(file.toPath());
                require(!Arrays.equals(first, second));
                require(Arrays.equals(vault.load(), SYNTHETIC));
                second[second.length - 1] ^= 1;
                Files.write(file.toPath(), second);
                mustReject(() -> vault.load());
                vault.save(SYNTHETIC);
                require(Arrays.equals(vault.load(), SYNTHETIC));
                byte[] invalidVersion = Files.readAllBytes(file.toPath()); invalidVersion[0] = 2;
                Files.write(file.toPath(), invalidVersion); mustReject(() -> vault.load());
                vault.save(SYNTHETIC);
                byte[] invalidNonce = Files.readAllBytes(file.toPath()); invalidNonce[1] = 0;
                Files.write(file.toPath(), invalidNonce); mustReject(() -> vault.load());
                mustReject(() -> vault.save(new byte[0]));
                mustReject(() -> vault.save(new byte[4097]));
                vault.clear(); require(vault.load() == null); require(!file.exists());
                KeyStore store = KeyStore.getInstance("AndroidKeyStore"); store.load(null);
                require(!store.containsAlias(ALIAS));
                ApiKeyVault session = new ApiKeyVault(getTargetContext(), "session");
                ApiKeyVault pending = new ApiKeyVault(getTargetContext(), "pending");
                require(Arrays.equals(session.load(), SYNTHETIC));
                require(Arrays.equals(pending.load(), SYNTHETIC));
                mustReject(() -> new ApiKeyVault(getTargetContext(), "../session"));
                mustReject(() -> session.save(new byte[16385]));
                File sessionFile = new File(getTargetContext().getNoBackupFilesDir(), "phishark-session.enc");
                File pendingFile = new File(getTargetContext().getNoBackupFilesDir(), "phishark-pending.enc");
                Files.write(pendingFile.toPath(), Files.readAllBytes(sessionFile.toPath()));
                mustReject(() -> pending.load()); // Different purpose uses a different key and AAD.
                pending.clear(); require(Arrays.equals(session.load(), SYNTHETIC));
                session.clear(); require(session.load() == null);
                Arrays.fill(first, (byte) 0); Arrays.fill(second, (byte) 0);
            }
            result.putInt("checks", checks); result.putString("outcome", "passed");
            finish(Activity.RESULT_OK, result);
        } catch (Throwable error) {
            try { vault.clear(); } catch (Exception ignored) { }
            result.putInt("checks", checks);
            result.putString("outcome", "failed");
            // Never print exception messages, plaintext, ciphertext or key material.
            result.putString("failureType", error.getClass().getSimpleName());
            finish(Activity.RESULT_CANCELED, result);
        }
    }
}
