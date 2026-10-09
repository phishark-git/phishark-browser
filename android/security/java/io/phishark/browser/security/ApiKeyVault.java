// SPDX-License-Identifier: GPL-3.0-only
package io.phishark.browser.security;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Browser-process key storage. Never expose this object through a JS bridge. */
public final class ApiKeyVault {
    private static final String ALIAS = "io.phishark.browser.personal-api-key.v1";
    private static final byte[] AAD = ALIAS.getBytes(StandardCharsets.UTF_8);
    private final AtomicFile file;

    public ApiKeyVault(Context context) {
        // Android's backup service excludes this directory.
        file = new AtomicFile(new File(context.getNoBackupFilesDir(), "phishark-api-key.enc"));
    }

    private SecretKey key() throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (!store.containsAlias(ALIAS)) {
            KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
            generator.init(new KeyGenParameterSpec.Builder(ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setRandomizedEncryptionRequired(true).build());
            generator.generateKey();
        }
        return (SecretKey) store.getKey(ALIAS, null);
    }

    public synchronized void save(byte[] apiKey) throws Exception {
        if (apiKey.length == 0 || apiKey.length > 4096) throw new IllegalArgumentException("Invalid API key");
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, key()); cipher.updateAAD(AAD);
        byte[] ciphertext = cipher.doFinal(apiKey), iv = cipher.getIV();
        byte[] envelope = ByteBuffer.allocate(2 + iv.length + ciphertext.length)
            .put((byte) 1).put((byte) iv.length).put(iv).put(ciphertext).array();
        FileOutputStream output = file.startWrite();
        try { output.write(envelope); file.finishWrite(output); }
        catch (Exception error) { file.failWrite(output); throw error; }
        finally { Arrays.fill(ciphertext, (byte) 0); Arrays.fill(envelope, (byte) 0); }
    }

    /** Caller owns and must erase the returned byte array after header creation. */
    public synchronized byte[] load() throws Exception {
        if (!file.getBaseFile().exists()) return null;
        byte[] envelope = file.readFully();
        try {
            ByteBuffer input = ByteBuffer.wrap(envelope);
            if (input.remaining() < 30 || input.get() != 1) throw new IllegalStateException("Invalid vault");
            int size = Byte.toUnsignedInt(input.get());
            if (size != 12 || input.remaining() <= size + 16) throw new IllegalStateException("Invalid vault");
            byte[] iv = new byte[size], ciphertext = new byte[input.remaining() - size];
            input.get(iv); input.get(ciphertext);
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, iv)); cipher.updateAAD(AAD);
            return cipher.doFinal(ciphertext);
        } finally { Arrays.fill(envelope, (byte) 0); }
    }

    public synchronized void clear() throws Exception {
        file.delete();
        KeyStore store = KeyStore.getInstance("AndroidKeyStore"); store.load(null);
        if (store.containsAlias(ALIAS)) store.deleteEntry(ALIAS);
    }
}
