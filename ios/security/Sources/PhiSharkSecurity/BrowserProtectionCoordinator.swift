// SPDX-License-Identifier: MPL-2.0
import Foundation

public protocol BrowserScanning: Sendable {
    func scan(
        _ profile: ScanProfile,
        target: URL,
        evidence: Data?,
        isPrivate: Bool,
        consent: Bool
    ) async -> BrowserScanOutcome
    func closePrivateSession() async
}

extension BrowserAPIClient: BrowserScanning {}

public struct ProtectionUpdate: Sendable {
    public let generation: UInt64
    public let state: ProtectionState
    public let isCurrent: Bool
    public let lastSafeURL: URL?
}

@MainActor public final class BrowserProtectionCoordinator {
    private let scanner: any BrowserScanning
    private var sessions: [String: NavigationSession] = [:]
    private var pending: [String: Task<BrowserScanOutcome, Never>] = [:]
    private var pendingIDs: [String: UUID] = [:]
    private var privateClearTask: Task<Void, Never>?

    public init(scanner: any BrowserScanning) { self.scanner = scanner }

    @discardableResult public func begin(tabID: String, target: URL, isPrivate: Bool, consent: Bool) -> UInt64 {
        pending.removeValue(forKey: tabID)?.cancel()
        pendingIDs.removeValue(forKey: tabID)
        let session: NavigationSession
        if let current = sessions[tabID], current.isPrivate == isPrivate {
            session = current
        } else {
            sessions[tabID]?.close()
            session = NavigationSession(isPrivate: isPrivate)
            sessions[tabID] = session
        }
        session.consent = consent && !isPrivate
        return session.begin(target)
    }

    public func preflight(tabID: String, generation: UInt64) async -> ProtectionUpdate {
        await run(.preflight, tabID: tabID, generation: generation, evidence: nil)
    }

    public func deep(tabID: String, generation: UInt64, evidence: Data) async -> ProtectionUpdate {
        await run(.deep, tabID: tabID, generation: generation, evidence: evidence)
    }

    public func current(tabID: String) -> ProtectionUpdate? {
        guard let session = sessions[tabID] else { return nil }
        return .init(generation: session.generation, state: session.state,
                     isCurrent: true, lastSafeURL: session.lastSafeURL)
    }

    public func setConsent(_ consent: Bool, tabID: String) {
        guard let session = sessions[tabID] else { return }
        session.consent = consent && !session.isPrivate
    }

    public func close(tabID: String) {
        pending.removeValue(forKey: tabID)?.cancel()
        pendingIDs.removeValue(forKey: tabID)
        guard let session = sessions.removeValue(forKey: tabID) else { return }
        session.close()
        if session.isPrivate {
            let previous = privateClearTask
            privateClearTask = Task { [scanner] in
                await previous?.value
                await scanner.closePrivateSession()
            }
        }
    }

    private func run(_ profile: ScanProfile, tabID: String, generation: UInt64, evidence: Data?) async -> ProtectionUpdate {
        guard let session = sessions[tabID], session.generation == generation, let target = session.url else {
            return .init(generation: generation, state: .unverified, isCurrent: false, lastSafeURL: nil)
        }
        if profile == .deep && !session.canCapture {
            return .init(generation: generation, state: .unverified, isCurrent: true,
                         lastSafeURL: session.lastSafeURL)
        }
        pending.removeValue(forKey: tabID)?.cancel()
        let requestID = UUID()
        pendingIDs[tabID] = requestID
        let isPrivate = session.isPrivate
        let consent = session.consent
        let privateClearTask = isPrivate ? self.privateClearTask : nil
        let task = Task {
            await privateClearTask?.value
            if Task.isCancelled { return BrowserScanOutcome(state: .unverified) }
            return await scanner.scan(profile, target: target, evidence: evidence,
                                      isPrivate: isPrivate, consent: consent)
        }
        pending[tabID] = task
        let outcome = await task.value
        guard let current = sessions[tabID], current === session,
              current.generation == generation, pendingIDs[tabID] == requestID else {
            return .init(generation: generation, state: .unverified, isCurrent: false, lastSafeURL: nil)
        }
        pending[tabID] = nil
        pendingIDs[tabID] = nil
        _ = current.apply(outcome, profile: profile, generation: generation)
        return .init(generation: generation, state: current.state, isCurrent: true,
                     lastSafeURL: current.lastSafeURL)
    }
}
