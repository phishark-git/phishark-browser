// SPDX-License-Identifier: MPL-2.0
import Foundation
import XCTest
@testable import PhiSharkSecurity

private actor StubScanner: BrowserScanning {
    private(set) var calls = 0

    func scan(
        _ profile: ScanProfile,
        target: URL,
        evidence: Data?,
        isPrivate: Bool,
        consent: Bool
    ) async -> BrowserScanOutcome {
        calls += 1
        if target.host == "slow.fixture.invalid" {
            try? await Task.sleep(nanoseconds: 100_000_000)
            return .init(state: .blocked)
        }
        return .init(state: .safe)
    }

    func closePrivateSession() async {}
}

private actor ClearingScanner: BrowserScanning {
    private(set) var clearCompleted = false
    private(set) var scannedAfterClear = false

    func scan(_ profile: ScanProfile, target: URL, evidence: Data?,
              isPrivate: Bool, consent: Bool) async -> BrowserScanOutcome {
        scannedAfterClear = clearCompleted
        return .init(state: .safe)
    }

    func closePrivateSession() async {
        try? await Task.sleep(nanoseconds: 30_000_000)
        clearCompleted = true
    }
}

final class BrowserProtectionCoordinatorTests: XCTestCase {
    @MainActor func testOlderNavigationCannotBlockNewPage() async {
        let scanner = StubScanner()
        let coordinator = BrowserProtectionCoordinator(scanner: scanner)
        let old = coordinator.begin(tabID: "tab", target: URL(string: "https://slow.fixture.invalid")!,
                                    isPrivate: false, consent: true)
        let pending = Task { await coordinator.preflight(tabID: "tab", generation: old) }
        try? await Task.sleep(nanoseconds: 10_000_000)
        let current = coordinator.begin(tabID: "tab", target: URL(string: "https://safe.fixture.invalid")!,
                                        isPrivate: false, consent: true)
        let latest = await coordinator.preflight(tabID: "tab", generation: current)
        let stale = await pending.value
        XCTAssertTrue(latest.isCurrent)
        XCTAssertEqual(latest.state, .safe)
        XCTAssertFalse(stale.isCurrent)
        XCTAssertEqual(coordinator.current(tabID: "tab")?.state, .safe)
    }

    @MainActor func testPrivateModeNeverStartsDeep() async {
        let scanner = StubScanner()
        let coordinator = BrowserProtectionCoordinator(scanner: scanner)
        let generation = coordinator.begin(tabID: "private", target: URL(string: "https://fixture.invalid")!,
                                           isPrivate: true, consent: true)
        let update = await coordinator.deep(tabID: "private", generation: generation,
                                            evidence: Data(#"{"response":{"html":"safe"}}"#.utf8))
        let calls = await scanner.calls
        XCTAssertEqual(update.state, .unverified)
        XCTAssertEqual(calls, 0)
        coordinator.close(tabID: "private")
        XCTAssertNil(coordinator.current(tabID: "private"))
    }

    @MainActor func testPrivateCacheClearPrecedesNextPrivateScan() async {
        let scanner = ClearingScanner()
        let coordinator = BrowserProtectionCoordinator(scanner: scanner)
        _ = coordinator.begin(tabID: "old", target: URL(string: "https://fixture.invalid/old")!,
                              isPrivate: true, consent: false)
        coordinator.close(tabID: "old")
        let generation = coordinator.begin(tabID: "new", target: URL(string: "https://fixture.invalid/new")!,
                                           isPrivate: true, consent: false)
        let update = await coordinator.preflight(tabID: "new", generation: generation)
        XCTAssertEqual(update.state, .safe)
        let scannedAfterClear = await scanner.scannedAfterClear
        XCTAssertTrue(scannedAfterClear)
    }
}
