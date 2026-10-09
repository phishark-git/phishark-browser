// SPDX-License-Identifier: MPL-2.0
import Foundation
import XCTest
@testable import PhiSharkSecurity

private final class FixtureURLProtocol: URLProtocol {
    nonisolated(unsafe) static var requests = [URLRequest]()
    nonisolated(unsafe) static var targets = [String: Int]()
    private static let lock = NSLock()

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        var body = request.httpBody ?? Data()
        if body.isEmpty, let stream = request.httpBodyStream {
            stream.open()
            var buffer = [UInt8](repeating: 0, count: 4096)
            while stream.hasBytesAvailable {
                let count = stream.read(&buffer, maxLength: buffer.count)
                if count <= 0 { break }
                body.append(contentsOf: buffer[..<count])
            }
            stream.close()
        }
        let payload = (try? JSONSerialization.jsonObject(with: body)) as? [String: Any]
        let target = payload?["target"] as? String ?? ""
        Self.lock.lock()
        Self.requests.append(request)
        Self.targets[target, default: 0] += 1
        let attempt = Self.targets[target] ?? 0
        Self.lock.unlock()
        let isDeep = request.url?.path == "/api/v1/browser/deep"
        let code: Int
        let response: String
        if target.contains("quota") {
            code = 429
            response = #"{"code":"QUOTA_EXCEEDED"}"#
        } else if target.contains("server-error") {
            code = 503
            response = #"{"code":"TEMPORARY_UNAVAILABLE"}"#
        } else if target.contains("not-configured") {
            code = 503
            response = #"{"code":"BROWSER_NOT_CONFIGURED"}"#
        } else if target.contains("capacity") && attempt == 1 {
            code = 429
            response = #"{"code":"PROFILE_CAPACITY"}"#
        } else {
            code = 200
            let score = target.contains("block") ? 86 : (target.contains("warn") ? 42 : 0)
            response = #"{"success":true,"data":{"scan_profile":"\#(isDeep ? "browser_deep_scan" : "preflight")","status":"completed","risk_calculation":{"risk_score":\#(score)}}}"#
        }
        let http = HTTPURLResponse(url: request.url!, statusCode: code, httpVersion: "HTTP/1.1", headerFields: ["Content-Type": "application/json", "Retry-After": "0"])!
        client?.urlProtocol(self, didReceive: http, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: Data(response.utf8))
        client?.urlProtocolDidFinishLoading(self)
    }

    override func stopLoading() {}

    static func reset() {
        lock.lock()
        requests.removeAll()
        targets.removeAll()
        lock.unlock()
    }

    static func requestCount() -> Int {
        lock.lock()
        defer { lock.unlock() }
        return requests.count
    }

    static func lastRequest() -> URLRequest? {
        lock.lock()
        defer { lock.unlock() }
        return requests.last
    }
}

final class BrowserAPIClientTests: XCTestCase {
    private func makeClient() throws -> BrowserAPIClient {
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [FixtureURLProtocol.self]
        let session = URLSession(configuration: config)
        return try BrowserAPIClient(baseURL: URL(string: "https://api.phishark.io")!, apiKey: "fixture-only", session: session)
    }

    func testEnvelopeCacheAndQuota() async throws {
        FixtureURLProtocol.reset()
        let client = try makeClient()
        let safe = URL(string: "https://fixture.invalid/pages/safe#fragment")!
        let first = await client.scan(.preflight, target: safe)
        XCTAssertEqual(first.state, .safe)
        XCTAssertEqual(first.status, 200)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), 1)
        let second = await client.scan(.preflight, target: safe)
        XCTAssertEqual(second.state, .safe)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), 1)
        let blocked = await client.scan(.preflight, target: URL(string: "https://fixture.invalid/pages/block")!)
        XCTAssertEqual(blocked.state, .blocked)
        let quota = await client.scan(.preflight, target: URL(string: "https://fixture.invalid/pages/quota")!)
        XCTAssertEqual(quota.state, .serviceError)
        XCTAssertEqual(quota.code, "QUOTA_EXCEEDED")
        let capacity = await client.scan(.preflight, target: URL(string: "https://fixture.invalid/pages/capacity")!)
        XCTAssertEqual(capacity.state, .safe)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), 5)
        XCTAssertEqual(FixtureURLProtocol.lastRequest()?.url?.path, "/api/v1/browser/preflight")
        XCTAssertEqual(FixtureURLProtocol.lastRequest()?.value(forHTTPHeaderField: "X-API-Key"), "fixture-only")
    }

    func testPrivateModeCannotDeepScanAndTeardownClearsCache() async throws {
        FixtureURLProtocol.reset()
        let client = try makeClient()
        let url = URL(string: "https://fixture.invalid/pages/safe")!
        let evidence = Data(#"{"response":{"html":"<p>safe</p>"}}"#.utf8)
        let privateDeep = await client.scan(.deep, target: url, evidence: evidence, isPrivate: true, consent: true)
        XCTAssertEqual(privateDeep.state, .unverified)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), 0)
        _ = await client.scan(.preflight, target: url, isPrivate: true)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), 1)
        await client.closePrivateSession()
        _ = await client.scan(.preflight, target: url, isPrivate: true)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), 2)
        let normalDeep = await client.scan(.deep, target: url, evidence: evidence, consent: true)
        XCTAssertEqual(normalDeep.state, .safe)
        XCTAssertEqual(FixtureURLProtocol.lastRequest()?.url?.path, "/api/v1/browser/deep")
    }

    func testWarningErrorAndConsentPolicies() async throws {
        FixtureURLProtocol.reset()
        let client = try makeClient()
        let warning = await client.scan(.preflight, target: URL(string: "https://fixture.invalid/pages/warn")!)
        XCTAssertEqual(warning.state, .warning)
        let temporary = await client.scan(.preflight, target: URL(string: "https://fixture.invalid/server-error")!)
        XCTAssertEqual(temporary.state, .unverified)
        let configuration = await client.scan(.preflight, target: URL(string: "https://fixture.invalid/not-configured")!)
        XCTAssertEqual(configuration.state, .serviceError)
        let count = FixtureURLProtocol.requestCount()
        let evidence = Data(#"{"response":{"html":"<p>safe</p>"}}"#.utf8)
        let withoutConsent = await client.scan(.deep, target: URL(string: "https://fixture.invalid/pages/warn")!,
                                               evidence: evidence, consent: false)
        XCTAssertEqual(withoutConsent.state, .unverified)
        XCTAssertEqual(FixtureURLProtocol.requestCount(), count)
    }
}
