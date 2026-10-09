// swift-tools-version: 6.2
// SPDX-License-Identifier: MPL-2.0
import PackageDescription
let package = Package(name: "PhiSharkSecurity", platforms: [.iOS(.v15), .macOS(.v13)],
    products: [.library(name: "PhiSharkSecurity", targets: ["PhiSharkSecurity"])],
    targets: [.target(name: "PhiSharkSecurity"), .testTarget(name: "PhiSharkSecurityTests", dependencies: ["PhiSharkSecurity"])])
