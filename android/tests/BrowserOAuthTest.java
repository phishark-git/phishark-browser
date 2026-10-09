// SPDX-License-Identifier: GPL-3.0-only
import io.phishark.browser.security.BrowserOAuth;

public final class BrowserOAuthTest {
    private static int checks;
    private static void check(boolean value) { if (!value) throw new AssertionError(); checks++; }
    private interface Action { void run() throws Exception; }
    private static void rejects(Action action) throws Exception {
        boolean failed = false;
        try { action.run(); } catch (IllegalArgumentException expected) { failed = true; }
        check(failed);
    }
    public static void main(String[] args) throws Exception {
        check(BrowserOAuth.challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk").equals("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
        check(BrowserOAuth.random().length() == 43);
        check(!BrowserOAuth.random().equals(BrowserOAuth.random()));
        String valid = BrowserOAuth.CALLBACK + "?state=fixture&code=code-fixture";
        check(BrowserOAuth.code(valid, "fixture", 1000, 2000).equals("code-fixture"));
        for (String uri : new String[]{
                "io.phishark.app:/oauth/callback?state=fixture&code=x",
                "io.phishark.browser://oauth/callback?state=fixture&code=x",
                "io.phishark.browser:/oauth/%63allback?state=fixture&code=x",
                valid + "#fragment", valid + "&code=replay", valid + "&state=fixture",
                valid + "&error=denied", valid.replace("state=fixture", "state=wrong"),
                valid.replace("code=code-fixture", "code=")}) {
            rejects(() -> BrowserOAuth.code(uri,"fixture",1000,2000));
        }
        rejects(() -> BrowserOAuth.code(valid,"fixture",1000,301000));
        rejects(() -> BrowserOAuth.code(valid,"fixture",1000,999));
        BrowserOAuth.validateAuthURL("https://app.phishark.io/browser/connect?state=s&flow_id=f", "f", "s");
        for (String url : new String[]{"https://app.phishark.io.evil.invalid/browser/connect?state=s&flow_id=f",
                "https://user@app.phishark.io/browser/connect?state=s&flow_id=f",
                "https://app.phishark.io/browser/connect?state=s&flow_id=f&state=x",
                "https://app.phishark.io/mobile/connect?state=s&flow_id=f"}) {
            rejects(() -> BrowserOAuth.validateAuthURL(url,"f","s"));
        }
        System.out.println("Browser OAuth assertions passed: " + checks);
    }
}
