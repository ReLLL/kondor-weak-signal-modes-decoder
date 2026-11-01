import Foundation
import Darwin

@main struct InstallerTests {
    static let fm = FileManager.default
    static func check(_ value: @autoclosure () throws -> Bool, _ message: String) throws {
        guard try value() else { throw InstallError.message("Test failed: " + message) }
    }
    static func fixture(_ root: URL, standard: Bool = false, modes: Bool = false, js8: Bool = false, dsp: Bool = false) throws -> (PluginInstaller, URL, URL) {
        let profile = root.appendingPathComponent("Profile with spaces and ü")
        let payload = root.appendingPathComponent("Payload")
        let host = root.appendingPathComponent("SDR++Brown.app")
        for path in [profile, payload, host.appendingPathComponent("Contents/Frameworks")] {
            try fm.createDirectory(at: path, withIntermediateDirectories: true)
        }
        let core = Data("compatible host core".utf8)
        try core.write(to: host.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib"))
        let bundleID = standard ? "oorg.sdrpp.sdrpp" : "oorg.sdrpp.sdrppbrown"
        let version = standard ? "1.2.0" : "1.2.1"
        let plist: [String: Any] = ["CFBundleIdentifier": bundleID, "CFBundleVersion": version]
        try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
            .write(to: host.appendingPathComponent("Contents/Info.plist"))
        var files: [String: String] = [:]
        for name in PluginInstaller.names + (standard ? ["cty.dat"] : []) + (modes ? PluginInstaller.modeNames : []) + (js8 ? PluginInstaller.js8Names : []) + (dsp ? PluginInstaller.dspNames : []) {
            let data = Data(("new-" + name).utf8)
            try data.write(to: payload.appendingPathComponent(name)); files[name] = PluginInstaller.hash(data)
        }
        let manifest = PayloadManifest(version: "1.0.25", hostBundleID: bundleID,
            hostVersion: version, hostCoreSHA256: PluginInstaller.hash(core), hostLibraries: ["libsdrpp_core.dylib"], files: files)
        try JSONEncoder().encode(manifest).write(to: payload.appendingPathComponent("manifest.json"))
        let config: [String: Any] = ["operatorLocation": "KM69", "modules": ["other.dylib"],
            "moduleInstances": ["Radio": ["module": "radio", "enabled": true]],
            "menuElements": [["name": "Radio", "open": false]], "custom": ["unchanged": true]]
        try JSONSerialization.data(withJSONObject: config).write(to: profile.appendingPathComponent("config.json"))
        try Data("preserve reception history".utf8).write(to: profile.appendingPathComponent("history.sqlite3"))
        var engine = try PluginInstaller(payload: payload, brownRunning: { false })
        // Transaction fixtures use text, not executable Mach-O files. Real loader
        // failures and runtime isolation are covered by test_host_compatibility.py.
        engine.compatibilityCheck = { _, _, _ in }
        return (engine, profile, host)
    }
    static func scenario(_ name: String, body: (URL) throws -> Void) throws {
        let root = fm.temporaryDirectory.appendingPathComponent("dx-installer-test-" + UUID().uuidString)
        try fm.createDirectory(at: root, withIntermediateDirectories: false)
        defer { try? fm.removeItem(at: root) }
        try body(root); print("PASS " + name)
    }
    static func expectFailure(_ body: () throws -> Void) throws {
        var failed = false
        do { try body() } catch { failed = true }
        try check(failed, "expected failure")
    }
    static func main() throws {
        try scenario("private DSP libraries are installed as one verified group") { root in
            let (engine, profile, host) = try fixture(root, dsp: true)
            _ = try engine.install(profile: profile, host: host)
            for name in PluginInstaller.dspNames {
                let target = profile.appendingPathComponent("plugins/dx-notebook/" + name)
                try check(Data(contentsOf: target) == Data("new-\(name)".utf8), "private runtime copied")
                if name == "dx_host_probe" || name.hasSuffix(".dylib") {
                    try check((fm.attributesOfItem(atPath: target.path)[.posixPermissions] as! NSNumber).intValue == 0o755, "runtime executable")
                }
            }
            try fm.removeItem(at: engine.payload.appendingPathComponent("libkondor_volk.dylib"))
            try expectFailure { try engine.validatePayload() }
        }
        for issue in ["architecture", "missing symbol", "newer OS", "helper runtime"] {
            try scenario("loader rejects \(issue) before any profile mutation") { root in
                var (engine, profile, host) = try fixture(root)
                let config = try Data(contentsOf: profile.appendingPathComponent("config.json"))
                engine.compatibilityCheck = { _, _, _ in throw InstallError.message(issue) }
                try expectFailure { _ = try engine.install(profile: profile, host: host) }
                try check(Data(contentsOf: profile.appendingPathComponent("config.json")) == config, "settings untouched")
                try check(!fm.fileExists(atPath: profile.appendingPathComponent("dx-notebook-backups").path), "no writes before loader check")
            }
        }
        try scenario("compatibility child failure and timeout are bounded") { root in
            let fail = root.appendingPathComponent("failure.sh")
            try Data("#!/bin/sh\necho missing-symbol >&2\nexit 5\n".utf8).write(to: fail)
            try fm.setAttributes([.posixPermissions: 0o755], ofItemAtPath: fail.path)
            do {
                try HostCompatibility.run(fail, arguments: [])
                throw InstallError.message("expected runtime rejection")
            } catch { try check(error.localizedDescription.contains("missing-symbol"), "loader details retained") }
            // A single process avoids leaving child processes after the timeout.
            try Data("#!/bin/sh\nexec /bin/sleep 20\n".utf8).write(to: fail)
            let start = Date()
            try expectFailure { try HostCompatibility.run(fail, arguments: [], timeout: 0.1) }
            try check(Date().timeIntervalSince(start) < 2, "checker killed promptly")
        }
        for checkpoint in ["staged", "plugin-replaced"] {
            try scenario("host update during \(checkpoint) preserves settings and rolls back") { root in
                var (engine, profile, host) = try fixture(root)
                let config = try Data(contentsOf: profile.appendingPathComponent("config.json"))
                engine.checkpoint = { if $0 == checkpoint {
                    try Data("updated host".utf8).write(to: host.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib"))
                } }
                try expectFailure { _ = try engine.install(profile: profile, host: host) }
                try check(Data(contentsOf: profile.appendingPathComponent("config.json")) == config, "config preserved")
                try check(!fm.fileExists(atPath: profile.appendingPathComponent("plugins/dx-notebook").path), "candidate removed")
            }
        }
        for standard in [false, true] {
            for oldName in PluginInstaller.previousNames {
                try scenario("rename from \(oldName) preserves receiver settings and sidebar") { root in
                    let (engine, profile, host) = try fixture(root, standard: standard)
                    let config = profile.appendingPathComponent("config.json")
                    var data = try JSONSerialization.jsonObject(with: Data(contentsOf: config)) as! [String: Any]
                    data["moduleInstances"] = [oldName: ["module": PluginInstaller.module, "enabled": false, "receiverGrid": "KM69KW", "receiverCall": "TA1ABC"]]
                    data["menuElements"] = [["name": oldName, "open": false], ["name": oldName, "open": true]]
                    try JSONSerialization.data(withJSONObject: data).write(to: config)
                    _ = try engine.install(profile: profile, host: host)
                    let updated = try JSONSerialization.jsonObject(with: Data(contentsOf: config)) as! [String: Any]
                    let instances = updated["moduleInstances"] as! [String: [String: Any]]
                    try check(instances[oldName] == nil && instances.count == 1, "old registration removed")
                    try check(instances[PluginInstaller.instance]?["receiverGrid"] as? String == "KM69KW", "grid preserved")
                    try check(instances[PluginInstaller.instance]?["receiverCall"] as? String == "TA1ABC", "call preserved")
                    try check(instances[PluginInstaller.instance]?["enabled"] as? Bool == false, "disabled choice preserved")
                    let menu = updated["menuElements"] as! [[String: Any]]
                    try check(menu.count == 1 && menu[0]["name"] as? String == PluginInstaller.instance && menu[0]["open"] as? Bool == false, "sidebar renamed once")
                    let first = try Data(contentsOf: config)
                    _ = try engine.install(profile: profile, host: host)
                    try check(Data(contentsOf: config) == first, "repeated rename keeps settings stable")
                }
            }
        }
        for standard in [false, true] {
            try scenario("optional JS8 payload, permissions and incomplete-payload refusal") { root in
                let (engine, profile, host) = try fixture(root, standard: standard, modes: true, js8: true)
                _ = try engine.install(profile: profile, host: host)
                for name in PluginInstaller.js8Names {
                    let target = profile.appendingPathComponent("plugins/dx-notebook/" + name)
                    try check(Data(contentsOf: target) == Data("new-\(name)".utf8), "JS8 payload preserved")
                    if name == "dx_js8_decoder" { try check((fm.attributesOfItem(atPath: target.path)[.posixPermissions] as! NSNumber).intValue == 0o755, "JS8 helper executable") }
                }
                try fm.removeItem(at: engine.payload.appendingPathComponent("COPYING.JS8"))
                try expectFailure { try engine.validatePayload() }
            }
            try scenario("native mode payload, runtimes and executable permissions") { root in
                let (engine, profile, host) = try fixture(root, standard: standard, modes: true)
                _ = try engine.install(profile: profile, host: host)
                for name in PluginInstaller.modeNames {
                    let target = profile.appendingPathComponent("plugins/dx-notebook/" + name)
                    try check(Data(contentsOf: target) == Data("new-\(name)".utf8), "native payload copied exactly")
                    if name.hasSuffix(".dylib") || name.hasPrefix("dx_") { try check((fm.attributesOfItem(atPath: target.path)[.posixPermissions] as! NSNumber).intValue == 0o755, "native code executable") }
                }
                try fm.removeItem(at: engine.payload.appendingPathComponent("libgfortran.5.dylib"))
                try expectFailure { try engine.validatePayload() }
            }
        }
        try scenario("location grids, precision, stale and unreliable estimates") { _ in
            try check(GridLocator.coordinate(latitude: 39.93, longitude: 32.86) == "KM69KW", "Ankara six-character grid")
            try check(GridLocator.coordinate(latitude: 0, longitude: 0) == "JJ00AA", "equator and meridian")
            try check(GridLocator.coordinate(latitude: 90, longitude: 180) == "RR99XX", "upper coordinate bounds")
            try check(GridLocator.coordinate(latitude: -90, longitude: -180) == "AA00AA", "lower bounds")
            try check(GridLocator.suggestion(latitude: 39.93, longitude: 32.86, accuracy: 9000, age: 30) == "KM69", "coarse estimate gives four characters")
            try check(GridLocator.suggestion(latitude: 39.93, longitude: 32.86, accuracy: 500, age: 30) == "KM69KW", "accurate estimate gives six characters")
            try check(GridLocator.suggestion(latitude: 39.93, longitude: 32.86, accuracy: 21_000, age: 30) == nil, "reject unreliable estimate")
            try check(GridLocator.suggestion(latitude: 39.93, longitude: 32.86, accuracy: 500, age: 301) == nil, "reject stale estimate")
            try check(GridLocator.coordinate(latitude: .nan, longitude: 0) == nil && GridLocator.coordinate(latitude: 91, longitude: 0) == nil, "reject invalid coordinates")
        }
        try scenario("fresh install, backups, permissions, logs and repeated upgrades") { root in
            let (engine, profile, host) = try fixture(root)
            let config = profile.appendingPathComponent("config.json")
            let original = try Data(contentsOf: config)
            let receipt = try engine.install(profile: profile, host: host)
            try check(Data(contentsOf: receipt.backup.appendingPathComponent("config.json")) == original, "backup exact")
            for _ in 0..<2 { _ = try engine.install(profile: profile, host: host) }
            let data = try JSONSerialization.jsonObject(with: Data(contentsOf: config)) as! [String: Any]
            try check((data["modules"] as! [String]).count == 2, "registration not duplicated")
            try check(data["operatorLocation"] as? String == "KM69", "home grid retained")
            try check((data["custom"] as! [String: Bool])["unchanged"] == true, "foreign settings retained")
            try check(Data(contentsOf: profile.appendingPathComponent("history.sqlite3")) == Data("preserve reception history".utf8), "logs retained")
            let decoder = profile.appendingPathComponent("plugins/dx-notebook/dx_decoder")
            try check((fm.attributesOfItem(atPath: decoder.path)[.posixPermissions] as! NSNumber).intValue == 0o755, "helper executable")
        }
        for point in ["staged", "plugin-replaced", "configuration-replaced"] {
            try scenario("rollback at " + point) { root in
                var (engine, profile, host) = try fixture(root)
                let target = profile.appendingPathComponent("plugins/dx-notebook")
                try fm.createDirectory(at: target, withIntermediateDirectories: true)
                try Data("old plugin".utf8).write(to: target.appendingPathComponent("dx_notebook.dylib"))
                let original = try Data(contentsOf: profile.appendingPathComponent("config.json"))
                engine.checkpoint = { if $0 == point { throw InstallError.message("injected write failure") } }
                try expectFailure { _ = try engine.install(profile: profile, host: host) }
                try check(Data(contentsOf: profile.appendingPathComponent("config.json")) == original, "configuration restored")
                try check(Data(contentsOf: target.appendingPathComponent("dx_notebook.dylib")) == Data("old plugin".utf8), "plugin restored")
                try check(!fm.contentsOfDirectory(atPath: profile.path).contains(where: { $0.hasPrefix(".dx-install-") }), "stage cleaned")
            }
        }
        for issue in ["running", "corrupt payload", "wrong host", "missing library", "invalid configuration", "symlink"] {
            try scenario("reject " + issue + " without profile changes") { root in
                var (engine, profile, host) = try fixture(root)
                let config = profile.appendingPathComponent("config.json")
                if issue == "running" { engine.brownRunning = { true } }
                if issue == "corrupt payload" { try Data("bad".utf8).write(to: engine.payload.appendingPathComponent("dx_decoder")) }
                if issue == "wrong host" { try PropertyListSerialization.data(fromPropertyList: ["CFBundleIdentifier": "other.app"], format: .xml, options: 0).write(to: host.appendingPathComponent("Contents/Info.plist")) }
                if issue == "missing library" { try fm.removeItem(at: host.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib")) }
                if issue == "invalid configuration" { try Data("{\"modules\":42}".utf8).write(to: config) }
                if issue == "symlink" { try fm.createSymbolicLink(at: profile.appendingPathComponent("plugins"), withDestinationURL: root) }
                let original = try Data(contentsOf: config)
                try expectFailure { _ = try engine.install(profile: profile, host: host) }
                try check(Data(contentsOf: config) == original, "config untouched")
                try check(!fm.fileExists(atPath: profile.appendingPathComponent("dx-notebook-backups").path), "no writes before validation")
            }
        }
        try scenario("configuration changes during staging abort safely") { root in
            var (engine, profile, host) = try fixture(root)
            let changed = Data("{\"new_setting\":true}".utf8)
            engine.checkpoint = { if $0 == "staged" { try changed.write(to: profile.appendingPathComponent("config.json")) } }
            try expectFailure { _ = try engine.install(profile: profile, host: host) }
            try check(Data(contentsOf: profile.appendingPathComponent("config.json")) == changed, "new external settings preserved")
            try check(!fm.fileExists(atPath: profile.appendingPathComponent("plugins/dx-notebook").path), "plugin untouched")
        }
        try scenario("standard host payload, receiver setup and disabled preference") { root in
            let (engine, profile, host) = try fixture(root, standard: true)
            let file = profile.appendingPathComponent("config.json")
            var config = try JSONSerialization.jsonObject(with: Data(contentsOf: file)) as! [String: Any]
            config["moduleInstances"] = [PluginInstaller.instance: ["module": PluginInstaller.module,
                "enabled": false, "receiverGrid": "FN42", "receiverCall": "K1ABC"]]
            try JSONSerialization.data(withJSONObject: config).write(to: file)
            _ = try engine.install(profile: profile, host: host, homeGrid: "KM69xx")
            let installed = try JSONSerialization.jsonObject(with: Data(contentsOf: file)) as! [String: Any]
            let value = (installed["moduleInstances"] as! [String: [String: Any]])[PluginInstaller.instance]!
            try check(value["receiverGrid"] as? String == "KM69XX", "home locator applied")
            try check(value["receiverCall"] as? String == "K1ABC", "other receiver fields retained")
            try check(value["enabled"] as? Bool == false, "disabled preference retained")
            try check(fm.fileExists(atPath: profile.appendingPathComponent("plugins/dx-notebook/cty.dat").path), "country database installed")
        }
        try scenario("invalid home locator changes nothing") { root in
            let (engine, profile, host) = try fixture(root)
            let original = try Data(contentsOf: profile.appendingPathComponent("config.json"))
            try expectFailure { _ = try engine.install(profile: profile, host: host, homeGrid: "WRONG") }
            try check(Data(contentsOf: profile.appendingPathComponent("config.json")) == original, "settings unchanged")
        }
        for standard in [false,true] { try scenario("accept different host versions and rebuilt core") { root in
            let (engine,profile,host) = try fixture(root,standard: standard)
            let plist = ["CFBundleIdentifier": engine.manifest.hostBundleID, "CFBundleVersion": "9.9.9"]
            try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0).write(to: host.appendingPathComponent("Contents/Info.plist"))
            try Data("rebuilt core".utf8).write(to: host.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib"))
            _ = try engine.install(profile: profile, host: host)
        } }
        print("Installer safety checks passed.")
    }
}
