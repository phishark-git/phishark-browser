// SPDX-License-Identifier: MPL-2.0
import CryptoKit
import Foundation

public struct BrowserScanOutcome: Sendable {
    public let state: ProtectionState
    public let result: ScanResult?
    public let status: Int?
    public let code: String?

    public init(state: ProtectionState, result: ScanResult? = nil, status: Int? = nil, code: String? = nil) {
        self.state = state
        self.result = result
        self.status = status
        self.code = code
    }
}

public enum BrowserClientError: Error {
    case invalidBaseURL
    case invalidKey
}

private final class NoRedirectDelegate: NSObject, URLSessionTaskDelegate, @unchecked Sendable {
    func urlSession(
        _ session: URLSession,
        task: URLSessionTask,
        willPerformHTTPRedirection response: HTTPURLResponse,
        newRequest request: URLRequest,
        completionHandler: @escaping (URLRequest?) -> Void
    ) {
        completionHandler(nil)
    }
}

public actor BrowserAPIClient {
    private struct CacheKey: Hashable {
        let profile: ScanProfile
        let url: String
        let evidenceHash: String
        let isPrivate: Bool
    }

    private struct CacheEntry {
        let value: BrowserScanOutcome
        let expires: Date
    }

    private let baseURL: URL
    private let apiKey: String
    private let session: URLSession
    private var cache: [CacheKey: CacheEntry] = [:]
    private var inFlight: [CacheKey: Task<BrowserScanOutcome, Never>] = [:]
    private var privateEpoch: UInt64 = 0

    public init(baseURL: URL, apiKey: String, allowLoopbackHTTP: Bool = false) throws {
        try self.init(baseURL: baseURL, apiKey: apiKey, allowLoopbackHTTP: allowLoopbackHTTP, session: nil)
    }

    init(baseURL: URL, apiKey: String, allowLoopbackHTTP: Bool = false, session: URLSession?) throws {
        #if DEBUG
        let loopbackHTTPAllowed = allowLoopbackHTTP
        #else
        let loopbackHTTPAllowed = false
        #endif
        guard let components = URLComponents(url: baseURL, resolvingAgainstBaseURL: false),
              let scheme = components.scheme?.lowercased(),
              let host = components.host?.lowercased(),
              components.user == nil, components.password == nil,
              components.query == nil, components.fragment == nil,
              components.path.isEmpty || components.path == "/",
              scheme == "https" || (loopbackHTTPAllowed && scheme == "http" && ["127.0.0.1", "localhost"].contains(host))
        else { throw BrowserClientError.invalidBaseURL }
        guard !apiKey.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              !apiKey.contains("\r"), !apiKey.contains("\n") else { throw BrowserClientError.invalidKey }
        self.baseURL = baseURL
        self.apiKey = apiKey
        let config = URLSessionConfiguration.ephemeral
        config.httpCookieStorage = nil
        config.httpShouldSetCookies = false
        config.urlCache = nil
        config.requestCachePolicy = .reloadIgnoringLocalCacheData
        config.timeoutIntervalForResource = 20
        self.session = session ?? URLSession(configuration: config, delegate: NoRedirectDelegate(), delegateQueue: nil)
    }

    public func scan(
        _ profile: ScanProfile,
        target: URL,
        evidence: Data? = nil,
        isPrivate: Bool = false,
        consent: Bool = false
    ) async -> BrowserScanOutcome {
        if Task.isCancelled { return .init(state: .unverified) }
        guard let canonical = Self.canonicalTarget(target) else { return .init(state: .unverified) }
        if profile == .deep && (isPrivate || !consent || evidence == nil) { return .init(state: .unverified) }
        if profile == .preflight && evidence != nil { return .init(state: .unverified) }
        if let evidence, evidence.count > 9_437_184 { return .init(state: .unverified) }
        let evidenceHash = evidence.map { Data(SHA256.hash(data: $0)).base64EncodedString() } ?? ""
        let key = CacheKey(profile: profile, url: canonical, evidenceHash: evidenceHash, isPrivate: isPrivate)
        if let entry = cache[key], entry.expires > Date() { return entry.value }
        if let task = inFlight[key] { return await task.value }
        let epoch = privateEpoch
        let task = Task { [self] in
            await self.perform(profile, canonical: canonical, evidence: evidence)
        }
        inFlight[key] = task
        let outcome = await task.value
        inFlight[key] = nil
        if (!isPrivate || epoch == privateEpoch), [.safe, .warning, .blocked].contains(outcome.state) {
            cache[key] = CacheEntry(value: outcome, expires: Date().addingTimeInterval(profile == .preflight ? 600 : 120))
        }
        return outcome
    }

    public func closePrivateSession() {
        privateEpoch &+= 1
        for (key, task) in inFlight where key.isPrivate { task.cancel() }
        inFlight = inFlight.filter { !$0.key.isPrivate }
        cache = cache.filter { !$0.key.isPrivate }
    }

    public func clearCache() {
        for task in inFlight.values { task.cancel() }
        inFlight.removeAll()
        cache.removeAll()
        privateEpoch &+= 1
    }

    private func perform(_ profile: ScanProfile, canonical: String, evidence: Data?) async -> BrowserScanOutcome {
        let nanoseconds: UInt64 = profile == .preflight ? 10_000_000_000 : 20_000_000_000
        return await withTaskGroup(of: BrowserScanOutcome.self) { group in
            group.addTask { await self.execute(profile, canonical: canonical, evidence: evidence) }
            group.addTask {
                try? await Task.sleep(nanoseconds: nanoseconds)
                return .init(state: .unverified)
            }
            let first = await group.next() ?? .init(state: .unverified)
            group.cancelAll()
            return first
        }
    }

    private func execute(_ profile: ScanProfile, canonical: String, evidence: Data?) async -> BrowserScanOutcome {
        let budget = profile == .preflight ? 10.0 : 20.0
        let deadline = Date().addingTimeInterval(budget)
        var body: [String: Any] = ["target": canonical]
        if let evidence {
            guard let value = try? JSONSerialization.jsonObject(with: evidence),
                  let object = value as? [String: Any],
                  let response = object["response"] as? [String: Any],
                  response["html"] != nil || response["screenshot"] != nil
            else { return .init(state: .unverified) }
            body["web_evidence"] = object
        }
        guard let bodyData = try? JSONSerialization.data(withJSONObject: body) else { return .init(state: .unverified) }
        let path = profile == .preflight ? "preflight" : "deep"
        guard let url = URL(string: "/api/v1/browser/\(path)", relativeTo: baseURL)?.absoluteURL else {
            return .init(state: .unverified)
        }
        for attempt in 0..<2 {
            if Task.isCancelled || Date() >= deadline { return .init(state: .unverified) }
            var request = URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData)
            request.httpMethod = "POST"
            request.httpBody = bodyData
            request.timeoutInterval = max(0.1, deadline.timeIntervalSinceNow)
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
            request.setValue(apiKey, forHTTPHeaderField: "X-API-Key")
            do {
                let (bytes, response) = try await session.bytes(for: request)
                guard let http = response as? HTTPURLResponse else { return .init(state: .unverified) }
                var data = Data()
                for try await byte in bytes {
                    if Task.isCancelled || Date() >= deadline || data.count >= 1_048_576 {
                        return .init(state: .unverified)
                    }
                    data.append(byte)
                }
                if http.statusCode == 429 {
                    let code = Self.errorCode(data)
                    if Self.quotaCodes.contains(code ?? "") { return .init(state: .serviceError, status: 429, code: code) }
                    if attempt == 0 {
                        let delay = min(2.0, max(0, Double(http.value(forHTTPHeaderField: "Retry-After") ?? "") ?? 0))
                        if Date().addingTimeInterval(delay) >= deadline { return .init(state: .unverified, status: 429) }
                        try await Task.sleep(nanoseconds: UInt64(delay * 1_000_000_000))
                        continue
                    }
                    return .init(state: .unverified, status: 429)
                }
                if !(200..<300).contains(http.statusCode) {
                    let code = Self.errorCode(data)
                    let temporary = [500, 502, 503, 504].contains(http.statusCode) && code != "BROWSER_NOT_CONFIGURED"
                    return .init(state: temporary ? .unverified : .serviceError, status: http.statusCode, code: code)
                }
                let decoder = JSONDecoder()
                decoder.keyDecodingStrategy = .convertFromSnakeCase
                let result = (try? decoder.decode(Envelope.self, from: data).data) ?? (try? ScanResult.decode(data))
                guard let result, !Task.isCancelled, Date() < deadline else { return .init(state: .unverified) }
                return .init(state: result.decision(for: profile), result: result, status: http.statusCode)
            } catch {
                return .init(state: .unverified)
            }
        }
        return .init(state: .unverified)
    }

    private struct Envelope: Decodable { let data: ScanResult }
    private static let quotaCodes = ["QUOTA_EXCEEDED", "LIMIT_EXCEEDED", "INSUFFICIENT_CREDITS"]

    private static func errorCode(_ data: Data) -> String? {
        (try? JSONSerialization.jsonObject(with: data) as? [String: Any])?["code"] as? String
    }

    private static func canonicalTarget(_ url: URL) -> String? {
        guard var components = URLComponents(url: url, resolvingAgainstBaseURL: false),
              let scheme = components.scheme?.lowercased(), ["http", "https"].contains(scheme),
              components.host != nil, components.user == nil, components.password == nil
        else { return nil }
        components.fragment = nil
        return components.url?.absoluteString
    }
}
