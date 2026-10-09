// SPDX-License-Identifier: MPL-2.0
import Foundation
import CryptoKit
import Security

/// PKCE contract for the native ASWebAuthenticationSession adapter on Mac/iOS.
/// Store this object in Keychain; never in renderer storage or a JavaScript bridge.
public struct BrowserAccountFlow: Codable {
    public static let callback = "io.phishark.browser:/oauth/callback"
    public static let authBase = "https://api.phishark.io/api/browser/auth"
    public let verifier: String
    public let state: String
    public let createdAt: Date
    public init(now: Date = Date()) throws {
        verifier = try Self.random(); state = try Self.random(); createdAt = now
    }
    public var challenge: String {
        Self.base64url(Data(SHA256.hash(data: Data(verifier.utf8))))
    }
    public func authorizationCode(from url: URL, now: Date = Date()) throws -> String {
        guard now >= createdAt, now.timeIntervalSince(createdAt) < 300,
              let parts = URLComponents(url: url, resolvingAgainstBaseURL: false),
              parts.scheme == "io.phishark.browser", parts.host == nil,
              parts.user == nil, parts.password == nil, parts.port == nil,
              parts.percentEncodedPath == "/oauth/callback", parts.fragment == nil else { throw FlowError.invalidCallback }
        let query = parts.queryItems ?? []
        guard query.count == 2, query.filter({ $0.name == "state" }).count == 1,
              query.first(where: { $0.name == "state" })?.value == state,
              query.filter({ $0.name == "code" }).count == 1,
              let code = query.first(where: { $0.name == "code" })?.value,
              !code.isEmpty, code.utf8.count <= 4096 else { throw FlowError.invalidCallback }
        return code
    }
    public func validateAuthorizationURL(_ url: URL, flowID: String) throws {
        guard let parts = URLComponents(url: url, resolvingAgainstBaseURL: false),
              parts.scheme == "https", parts.host == "app.phishark.io", parts.port == nil,
              parts.user == nil, parts.password == nil, parts.fragment == nil,
              parts.percentEncodedPath == "/browser/connect" else { throw FlowError.invalidCallback }
        let query = parts.queryItems ?? []
        guard query.count == 2, query.filter({$0.name == "state"}).count == 1,
              query.filter({$0.name == "flow_id"}).count == 1,
              query.first(where: {$0.name == "state"})?.value == state,
              query.first(where: {$0.name == "flow_id"})?.value == flowID else { throw FlowError.invalidCallback }
    }
    private static func random() throws -> String {
        var bytes = [UInt8](repeating: 0, count: 32)
        guard SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess else { throw FlowError.entropy }
        return base64url(Data(bytes))
    }
    private static func base64url(_ data: Data) -> String {
        data.base64EncodedString().replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_").replacingOccurrences(of: "=", with: "")
    }
    public enum FlowError: Error { case invalidCallback, entropy }
}
