// SPDX-License-Identifier: GPL-3.0-only
package org.chromium.base;
import android.content.Context;
import android.content.SharedPreferences;
/** Test-only adapter; uses only the isolated instrumentation app's context. */
public final class ContextUtils {
    private static Context context;
    public static void initialize(Context value) { context = value; }
    public static Context getApplicationContext() { return context; }
    public static SharedPreferences getAppSharedPreferences() { return context.getSharedPreferences("test-native-account", Context.MODE_PRIVATE); }
}
