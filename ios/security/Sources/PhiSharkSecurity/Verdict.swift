// SPDX-License-Identifier: MPL-2.0
import Foundation

public enum ScanProfile: String, Sendable { case preflight; case deep = "browser_deep_scan" }
public enum ProtectionState: String, Sendable { case checking, safe, warning, blocked, unverified, serviceError }
public struct ScanResult: Decodable, Sendable {
    public let scanProfile: String?
    public let status: String?
    public let verdict: String?
    public let riskScore: Double?
    public let riskCalculation: RiskCalculation?
    public let shortCircuitReason: String?
    public let analysisDegraded: Bool?
    public struct RiskCalculation: Decodable, Sendable { public let riskScore: Double? }
    public func decision(for profile: ScanProfile) -> ProtectionState {
        guard scanProfile == profile.rawValue else { return .unverified }
        let v = (verdict ?? "").trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        let reason = (shortCircuitReason ?? "").trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if ["unsafe", "malicious", "blocked", "phishing", "dangerous"].contains(v) ||
           reason.hasPrefix("gatekeeper_malicious:") || ["prompt_injection_detected", "prompt_injection_suspected"].contains(reason) { return .blocked }
        guard status == nil || status == "completed", let score = riskCalculation?.riskScore ?? riskScore,
              score.isFinite, score >= 0, score <= 100 else { return .unverified }
        if score >= (profile == .preflight ? 86 : 61) { return .blocked }
        if analysisDegraded == true { return .unverified }
        return score >= 31 ? .warning : .safe
    }
    public static func decode(_ data: Data) throws -> ScanResult {
        let decoder = JSONDecoder(); decoder.keyDecodingStrategy = .convertFromSnakeCase
        return try decoder.decode(ScanResult.self, from: data)
    }
}

@MainActor public final class NavigationSession {
    public private(set) var generation: UInt64 = 0
    public private(set) var state: ProtectionState = .unverified
    public private(set) var lastSafeURL: URL?
    public private(set) var url: URL?
    public let isPrivate: Bool
    public var consent = false
    private var tasks = [Task<Void, Never>]()
    public init(isPrivate: Bool) { self.isPrivate = isPrivate }
    @discardableResult public func begin(_ url: URL) -> UInt64 {
        tasks.forEach { $0.cancel() }; tasks.removeAll()
        generation &+= 1; self.url = url; state = .checking; return generation
    }
    public func track(_ task: Task<Void, Never>) { tasks.append(task) }
    @discardableResult public func apply(_ result: ScanResult, profile: ScanProfile, generation: UInt64) -> Bool {
        guard generation == self.generation, state != .blocked else { return false }
        state = result.decision(for: profile)
        if state == .safe && profile == .deep { lastSafeURL = url }
        return true
    }
    @discardableResult public func apply(_ outcome: BrowserScanOutcome, profile: ScanProfile, generation: UInt64) -> Bool {
        guard generation == self.generation, state != .blocked else { return false }
        state = outcome.state
        if state == .safe && profile == .deep { lastSafeURL = url }
        return true
    }
    public var canCapture: Bool { consent && !isPrivate && state != .blocked && state != .serviceError }
    public func close() {
        tasks.forEach { $0.cancel() }; tasks.removeAll(); generation &+= 1
        url = nil; lastSafeURL = nil; consent = false; state = .unverified
    }
}
