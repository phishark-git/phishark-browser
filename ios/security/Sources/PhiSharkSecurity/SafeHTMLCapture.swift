// SPDX-License-Identifier: MPL-2.0
import Foundation
import WebKit

@MainActor public enum SafeHTMLCapture {
    private static let script = #"""
    return (() => {
      const root = document.documentElement;
      if (!root) return null;
      const copy = root.cloneNode(true);
      copy.querySelectorAll('form,input,textarea,select,option,button,script,style,iframe,frame,object,embed,canvas,svg,video,audio,template,[contenteditable],[role="textbox"],[role="combobox"],[role="searchbox"],[aria-multiline],[tabindex]').forEach(node => node.remove());
      copy.querySelectorAll('*').forEach(node => {
        for (const attribute of Array.from(node.attributes)) node.removeAttribute(attribute.name);
      });
      return copy.outerHTML;
    })()
    """#

    public static func capture(from webView: WKWebView) async -> Data? {
        guard let html = try? await webView.callAsyncJavaScript(script, arguments: [:], in: nil,
                                                                contentWorld: .defaultClient) as? String,
              !html.isEmpty,
              html.utf8.count <= 3_145_728
        else { return nil }
        let evidence: [String: Any] = ["response": ["html": html]]
        guard let data = try? JSONSerialization.data(withJSONObject: evidence),
              data.count <= 9_437_184 else { return nil }
        return data
    }
}
