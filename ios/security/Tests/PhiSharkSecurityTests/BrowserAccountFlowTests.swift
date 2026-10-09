import XCTest
@testable import PhiSharkSecurity

final class BrowserAccountFlowTests: XCTestCase {
    func testStateExpiryAndExactCallback() throws {
        let now = Date(timeIntervalSince1970: 1000)
        let flow = try BrowserAccountFlow(now: now)
        XCTAssertEqual(flow.verifier.count, 43)
        XCTAssertEqual(flow.challenge.count, 43)
        let valid = "\(BrowserAccountFlow.callback)?state=\(flow.state)&code=fixture"
        XCTAssertEqual(try flow.authorizationCode(from: XCTUnwrap(URL(string: valid)), now: now), "fixture")
        for invalid in [valid + "&code=second", valid + "#fragment", valid.replacingOccurrences(of: "browser:", with: "app:"), valid.replacingOccurrences(of: flow.state, with: "wrong")] {
            XCTAssertThrowsError(try flow.authorizationCode(from: XCTUnwrap(URL(string: invalid)), now: now))
        }
        XCTAssertThrowsError(try flow.authorizationCode(from: XCTUnwrap(URL(string: valid)), now: now.addingTimeInterval(300)))
    }
}
