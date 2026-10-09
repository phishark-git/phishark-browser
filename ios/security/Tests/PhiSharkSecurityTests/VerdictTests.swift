// SPDX-License-Identifier: MPL-2.0
import XCTest
@testable import PhiSharkSecurity

final class VerdictTests: XCTestCase {
    func testSharedVectors() throws {
        let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../shared/test-vectors/decisions.json").standardized
        let vectors = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(contentsOf: root)) as? [[String: Any]])
        for vector in vectors {
            let profile = try XCTUnwrap(ScanProfile(rawValue: vector["profile"] as! String))
            let data = try JSONSerialization.data(withJSONObject: vector["response"]!)
            let decision: ProtectionState
            do { decision = try ScanResult.decode(data).decision(for: profile) }
            catch { decision = .unverified }
            XCTAssertEqual(decision.rawValue, vector["expected"] as? String, vector["name"] as! String)
        }
    }
    @MainActor func testPrivateAndStaleResults() throws {
        let tab = NavigationSession(isPrivate: true); tab.consent = true
        let old = tab.begin(URL(string: "https://first.example/")!)
        _ = tab.begin(URL(string: "https://second.example/")!)
        let threat = try ScanResult.decode(Data(#"{"scan_profile":"preflight","verdict":"malicious"}"#.utf8))
        XCTAssertFalse(tab.apply(threat, profile: .preflight, generation: old))
        XCTAssertFalse(tab.canCapture); tab.close(); XCTAssertNil(tab.url)
    }
}
