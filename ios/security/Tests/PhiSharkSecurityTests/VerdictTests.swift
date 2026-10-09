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
            do {
                let result = try ScanResult.decode(data)
                decision = result.decision(for: profile)
                XCTAssertEqual(result.trustedPreflight, vector["skip_deep"] as? Bool ?? false, vector["name"] as! String)
            }
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
    @MainActor func testAllowlistDoesNotTrustRedirect() throws {
        let tab = NavigationSession(isPrivate: false); tab.consent = true
        let allow = try ScanResult.decode(Data(#"{"scan_profile":"preflight","status":"completed","risk_score":0,"short_circuit_reason":"gatekeeper_benign:whitelist"}"#.utf8))
        let first = tab.begin(URL(string: "https://allowed.example/")!)
        XCTAssertTrue(tab.apply(allow, profile: .preflight, generation: first))
        XCTAssertFalse(tab.canCapture); XCTAssertNil(tab.lastSafeURL)
        let redirect = tab.begin(URL(string: "https://unknown.example/")!)
        XCTAssertFalse(tab.apply(allow, profile: .preflight, generation: first))
        let unknown = try ScanResult.decode(Data(#"{"scan_profile":"preflight","risk_score":0}"#.utf8))
        XCTAssertTrue(tab.apply(unknown, profile: .preflight, generation: redirect))
        XCTAssertTrue(tab.canCapture)
        let threat = try ScanResult.decode(Data(#"{"scan_profile":"browser_deep_scan","risk_score":61}"#.utf8))
        XCTAssertTrue(tab.apply(threat, profile: .deep, generation: redirect))
        XCTAssertFalse(tab.canCapture)
        XCTAssertFalse(tab.apply(allow, profile: .preflight, generation: redirect))
        tab.close(); XCTAssertNil(tab.lastSafeURL)
    }
}
