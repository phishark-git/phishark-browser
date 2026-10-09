// SPDX-License-Identifier: MPL-2.0
import Foundation
import Security

public struct APIKeyVault {
    public enum Purpose: String { case apiKey = "personal-api-key.v1", session = "session.v1", pending = "pending.v1" }
    private let purpose: Purpose
    public init(purpose: Purpose = .apiKey) { self.purpose = purpose }
    private var identity: [String: Any] { [kSecClass as String: kSecClassGenericPassword,
        kSecAttrService as String: "io.phishark.browser", kSecAttrAccount as String: purpose.rawValue,
        kSecAttrSynchronizable as String: false] }
    public func save(_ key: Data) throws {
        guard !key.isEmpty, key.count <= (purpose == .apiKey ? 4096 : 16384) else { throw VaultError.invalidKey }
        let status = SecItemUpdate(identity as CFDictionary, [kSecValueData as String: key] as CFDictionary)
        if status == errSecItemNotFound {
            var item = identity; item[kSecValueData as String] = key
            item[kSecAttrAccessible as String] = kSecAttrAccessibleWhenUnlockedThisDeviceOnly
            let added = SecItemAdd(item as CFDictionary, nil)
            guard added == errSecSuccess else { throw VaultError.storage(added) }
        } else if status != errSecSuccess { throw VaultError.storage(status) }
    }
    public func load() throws -> Data? {
        var query = identity; query[kSecReturnData as String] = true; query[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess else { throw VaultError.storage(status) }
        return result as? Data
    }
    public func clear() throws {
        let status = SecItemDelete(identity as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else { throw VaultError.storage(status) }
    }
    public enum VaultError: Error { case invalidKey; case storage(OSStatus) }
}
