// SPDX-License-Identifier: MPL-2.0
import WebKit
import XCTest
@testable import PhiSharkSecurity

final class SafeHTMLCaptureTests: XCTestCase {
    @MainActor func testRemovesEditableValuesAndFrames() async throws {
        let webView = WKWebView(frame: .init(x: 0, y: 0, width: 390, height: 844))
        webView.loadHTMLString("""
            <html><body><h1>Safe title</h1>
            <form><input value="fixture-private-form"><textarea>fixture-private-textarea</textarea></form>
            <div contenteditable="true">fixture-private-editable</div>
            <div role="textbox">fixture-private-role</div>
            <iframe srcdoc="fixture-private-frame"></iframe>
            <p data-secret="fixture-private-attribute">Public content</p>
            </body></html>
            """, baseURL: URL(string: "https://fixture.invalid"))
        for _ in 0..<50 {
            if !webView.isLoading, webView.url != nil { break }
            try await Task.sleep(nanoseconds: 100_000_000)
        }
        let captured = await SafeHTMLCapture.capture(from: webView)
        let data = try XCTUnwrap(captured)
        let html = String(decoding: data, as: UTF8.self)
        XCTAssertTrue(html.contains("Safe title"))
        XCTAssertTrue(html.contains("Public content"))
        XCTAssertFalse(html.contains("fixture-private-"))
        XCTAssertFalse(html.contains("iframe"))
    }
}
