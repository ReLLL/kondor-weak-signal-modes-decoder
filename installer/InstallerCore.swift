import Foundation
import CryptoKit
import Darwin

enum InstallError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case .message(let text) = self { return text }; return nil }
}

struct PayloadManifest: Codable {
    let version: String
    let hostBundleID: String
    let hostVersion: String
    let hostCoreSHA256: String
    let hostLibraries: [String]
    let files: [String: String]
    var hostName: String { hostBundleID == "oorg.sdrpp.sdrpp" ? "SDR++" : "SDR++Brown" }
    var appPath: String { "/Applications/" + hostName + ".app" }
    var profileFolder: String { hostBundleID == "oorg.sdrpp.sdrpp" ? "sdrpp" : "sdrpp-brown" }
}

struct InstallReceipt {
    let backup: URL
    let profile: URL
}

struct PluginInstaller {
    static let instance = "Kondor Weak Signal Modes Decoder"
    static let previousNames = ["Kondor Advanced Decoder", "Advanced FT8/FT4/WSPR DX Decoder", "Advanced FT8/FT4 DX Decoder", "DX Notebook"]
    static let module = "advanced_ft8_ft4_dx_decoder"
    static let names = ["dx_notebook.dylib", "dx_decoder", "world.geojson"]
    static let modeNames = ["dx_modes_decoder", "dx_wspr_decoder", "libgfortran.5.dylib", "libquadmath.0.dylib", "libgcc_s.1.1.dylib", "COPYING.RUNTIME", "COPYING.LIB", "mode-runtime.json"]
    static let js8Names = ["dx_js8_decoder", "COPYING.JS8", "COPYING.QT", "js8-runtime.json"]
    let manager = FileManager.default
    let payload: URL
    let manifest: PayloadManifest
    var brownRunning: () throws -> Bool
    var checkpoint: (String) throws -> Void = { _ in }
    static let dspNames = ["libkondor_volk.dylib", "libkondor_fftw3f.dylib", "COPYING.DSP", "dsp-runtime.json", "dx_host_probe"]
    var compatibilityCheck: (URL, URL, [String]) throws -> Void = { try HostCompatibility.validate(payload: $0, host: $1, names: $2) }
    var names: [String] { Self.names + (manifest.hostBundleID == "oorg.sdrpp.sdrpp" ? ["cty.dat"] : []) + (manifest.files["dx_modes_decoder"] != nil ? Self.modeNames : []) + (manifest.files["dx_js8_decoder"] != nil ? Self.js8Names : []) + (manifest.files["dsp-runtime.json"] != nil ? Self.dspNames : []) }

    static func hash(_ data: Data) -> String {
        SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
    }

    init(payload: URL, brownRunning: @escaping () throws -> Bool) throws {
        self.payload = payload
        manifest = try JSONDecoder().decode(PayloadManifest.self,
            from: Data(contentsOf: payload.appendingPathComponent("manifest.json")))
        self.brownRunning = brownRunning
    }

    func regularFile(_ url: URL) throws {
        let values = try url.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey])
        guard values.isRegularFile == true, values.isSymbolicLink != true else {
            throw InstallError.message("An installation file is missing or is a symbolic link.")
        }
    }

    func validatePayload() throws {
        guard Set(manifest.files.keys) == Set(names) else {
            throw InstallError.message("The installer package is incomplete. Download it again.")
        }
        for name in names {
            let source = payload.appendingPathComponent(name)
            try regularFile(source)
            guard Self.hash(try Data(contentsOf: source)) == manifest.files[name] else {
                throw InstallError.message("An installation file failed its integrity check. Download it again.")
            }
        }
    }

    func validateHost(_ app: URL) throws {
        guard let info = NSDictionary(contentsOf: app.appendingPathComponent("Contents/Info.plist")),
              info["CFBundleIdentifier"] as? String == manifest.hostBundleID else {
            throw InstallError.message("Install \(manifest.hostName) in Applications first.")
        }
        let core = app.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib")
        try regularFile(core)
        for name in manifest.hostLibraries {
            guard !name.contains("/"), name.hasSuffix(".dylib") else { throw InstallError.message("The installer package has an invalid library list.") }
            let library = app.appendingPathComponent("Contents/Frameworks/" + name)
            guard manager.fileExists(atPath: library.path) else {
                throw InstallError.message("\(manifest.hostName) is missing its required runtime \(name). Reinstall the complete radio app, then retry. Nothing was changed.")
            }
            try regularFile(library)
        }
        try compatibilityCheck(payload, app, names)
    }

    func validateHomeGrid(_ grid: String) throws {
        if grid.isEmpty { return }
        let pattern = manifest.profileFolder == "sdrpp" ? "^[A-R]{2}[0-9]{2}([A-X]{2}([0-9]{2})?)?$" : "^[A-R]{2}[0-9]{2}([A-X]{2})?$"
        guard grid.uppercased().range(of: pattern, options: .regularExpression) != nil else {
            throw InstallError.message("Enter a valid Maidenhead home locator, or leave it empty to set later.")
        }
    }

    func configuration(_ original: Data, target: URL) throws -> Data {
        guard var data = try JSONSerialization.jsonObject(with: original) as? [String: Any] else {
            throw InstallError.message("The radio application's configuration is not a JSON object.")
        }
        guard data["modules"] == nil || data["modules"] is [String],
              data["moduleInstances"] == nil || data["moduleInstances"] is [String: [String: Any]],
              data["menuElements"] == nil || data["menuElements"] is [[String: Any]] else {
            throw InstallError.message("The radio application's configuration has an unexpected format. Nothing was changed.")
        }
        var instances = data["moduleInstances"] as? [String: [String: Any]] ?? [:]
        let existing = instances[Self.instance]
        let previous = Self.previousNames.compactMap { instances[$0] }
        let legacy = previous.first
        if let value = existing, ![Self.module, "dx_notebook"].contains(value["module"] as? String ?? "") {
            throw InstallError.message("The plugin name is already used by another module. Nothing was changed.")
        }
        if previous.contains(where: { ![Self.module, "dx_notebook"].contains($0["module"] as? String ?? "") }) {
            throw InstallError.message("The old plugin name is used by another module. Nothing was changed.")
        }
        var modules = data["modules"] as? [String] ?? []
        let plugin = target.appendingPathComponent("dx_notebook.dylib").path
        modules = modules.filter { $0 != plugin }
        modules.append(plugin)
        var instance = existing ?? legacy ?? [:]
        instance["module"] = Self.module
        instance["enabled"] = instance["enabled"] as? Bool ?? true
        instances[Self.instance] = instance
        for name in Self.previousNames { instances.removeValue(forKey: name) }
        var seen = false
        var menu: [[String: Any]] = []
        for var item in data["menuElements"] as? [[String: Any]] ?? [] {
            if Self.previousNames.contains(item["name"] as? String ?? "") { item["name"] = Self.instance }
            if item["name"] as? String == Self.instance {
                if seen { continue }; seen = true
            }
            menu.append(item)
        }
        if !seen { menu.insert(["name": Self.instance, "open": true], at: 0) }
        data["modules"] = modules
        data["moduleInstances"] = instances
        data["menuElements"] = menu
        return try JSONSerialization.data(withJSONObject: data, options: [.prettyPrinted, .sortedKeys]) + Data([10])
    }

    func install(profile input: URL, host: URL, homeGrid: String? = nil) throws -> InstallReceipt {
        let profile = input.standardizedFileURL.resolvingSymlinksInPath()
        let config = profile.appendingPathComponent("config.json")
        guard manager.fileExists(atPath: config.path) else {
            throw InstallError.message("Open \(manifest.hostName) once, then quit it and try Install again to create its settings.")
        }
        try regularFile(config)
        try validatePayload()
        let hostCore = host.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib")
        let validatedCore = Self.hash(try Data(contentsOf: hostCore))
        try validateHost(host)
        guard try !brownRunning() else { throw InstallError.message("Quit \(manifest.hostName) before installing. It saves settings when it closes.") }
        let original = try Data(contentsOf: config)
        let target = profile.appendingPathComponent("plugins/dx-notebook")
        try rejectSymlink(profile.appendingPathComponent("plugins"))
        try rejectSymlink(target)
        var updated = try configuration(original, target: target)
        if let grid = homeGrid, !grid.isEmpty {
            let value = grid.uppercased()
            try validateHomeGrid(value)
            var config = try JSONSerialization.jsonObject(with: updated) as! [String: Any]
            if manifest.profileFolder == "sdrpp" {
                var instances = config["moduleInstances"] as! [String: [String: Any]]
                instances[Self.instance]!["receiverGrid"] = value; config["moduleInstances"] = instances
            } else { config["operatorLocation"] = value }
            updated = try JSONSerialization.data(withJSONObject: config, options: [.prettyPrinted, .sortedKeys]) + Data([10])
        }
        let backupRoot = profile.appendingPathComponent("dx-notebook-backups")
        try rejectSymlink(backupRoot)
        let formatter = ISO8601DateFormatter()
        let stamp = formatter.string(from: Date()).replacingOccurrences(of: ":", with: "-")
        let backup = backupRoot.appendingPathComponent(stamp + "-" + UUID().uuidString)
        try manager.createDirectory(at: backup, withIntermediateDirectories: true)
        try manager.copyItem(at: config, to: backup.appendingPathComponent("config.json"))
        let hadPlugin = manager.fileExists(atPath: target.path)
        if hadPlugin { try manager.copyItem(at: target, to: backup.appendingPathComponent("plugin")) }
        let stage = profile.appendingPathComponent(".dx-install-" + UUID().uuidString)
        try manager.createDirectory(at: stage, withIntermediateDirectories: false)
        defer { try? manager.removeItem(at: stage) }
        let stagedPlugin = stage.appendingPathComponent("plugin")
        if hadPlugin { try manager.copyItem(at: target, to: stagedPlugin) }
        else { try manager.createDirectory(at: stagedPlugin, withIntermediateDirectories: false) }
        for name in names {
            let destination = stagedPlugin.appendingPathComponent(name)
            if manager.fileExists(atPath: destination.path) { try manager.removeItem(at: destination) }
            try manager.copyItem(at: payload.appendingPathComponent(name), to: destination)
            guard Self.hash(try Data(contentsOf: destination)) == manifest.files[name] else {
                throw InstallError.message("A copied file failed verification. Your settings were not changed.")
            }
            try manager.setAttributes([.posixPermissions: name.hasSuffix(".dylib") || name == "dx_host_probe" || name == "dx_decoder" || name == "dx_modes_decoder" || name == "dx_wspr_decoder" || name == "dx_js8_decoder" ? 0o755 : 0o644], ofItemAtPath: destination.path)
        }
        let stagedConfig = stage.appendingPathComponent("config.json")
        try updated.write(to: stagedConfig)
        let permissions = try manager.attributesOfItem(atPath: config.path)[.posixPermissions] ?? 0o600
        try manager.setAttributes([.posixPermissions: permissions], ofItemAtPath: stagedConfig.path)
        try checkpoint("staged")
        guard Self.hash(try Data(contentsOf: hostCore)) == validatedCore else {
            throw InstallError.message("The radio app changed during installation. Nothing was replaced. Retry with the updated app.")
        }
        guard try !brownRunning(), try Data(contentsOf: config) == original else {
            throw InstallError.message("The radio application started or its settings changed during installation. Nothing was replaced. Quit it and retry.")
        }
        try manager.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
        let oldPlugin = stage.appendingPathComponent("previous-plugin")
        var movedOld = false, movedNew = false, committedConfig = false
        do {
            if hadPlugin { try manager.moveItem(at: target, to: oldPlugin); movedOld = true }
            try manager.moveItem(at: stagedPlugin, to: target); movedNew = true
            try checkpoint("plugin-replaced")
            guard Self.hash(try Data(contentsOf: hostCore)) == validatedCore else {
                throw InstallError.message("The radio app changed during installation. Installation was canceled; retry with the updated app.")
            }
            guard try !brownRunning(), try Data(contentsOf: config) == original else {
                throw InstallError.message("The radio application started or its settings changed. Installation was canceled.")
            }
            guard rename(stagedConfig.path, config.path) == 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
            committedConfig = true
            try checkpoint("configuration-replaced")
        } catch {
            do {
                if committedConfig, try Data(contentsOf: config) == updated {
                    try original.write(to: config, options: .atomic)
                    try manager.setAttributes([.posixPermissions: permissions], ofItemAtPath: config.path)
                }
                if movedNew { try manager.removeItem(at: target) }
                if movedOld { try manager.moveItem(at: oldPlugin, to: target) }
            } catch {
                throw InstallError.message("Installation failed and automatic recovery was incomplete. Restore the backup at \(backup.path).")
            }
            throw error
        }
        return InstallReceipt(backup: backup, profile: profile)
    }

    private func rejectSymlink(_ url: URL) throws {
        if (try? url.resourceValues(forKeys: [.isSymbolicLinkKey]).isSymbolicLink) == true {
            throw InstallError.message("The plugin or backup folder is a symbolic link. Nothing was changed.")
        }
    }
}
