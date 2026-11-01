import Foundation
import Darwin

enum HostCompatibility {
    // Files instead of pipes keep loader diagnostics bounded without pipe deadlocks.
    static func run(_ executable: URL, arguments: [String], timeout: TimeInterval = 30) throws {
        let log = FileManager.default.temporaryDirectory.appendingPathComponent("dx-host-check-" + UUID().uuidString)
        guard FileManager.default.createFile(atPath: log.path, contents: nil, attributes: [.posixPermissions: 0o600]) else {
            throw InstallError.message("Unable to create the compatibility-check log. Nothing was changed.")
        }
        defer { try? FileManager.default.removeItem(at: log) }
        let output = try FileHandle(forWritingTo: log)
        defer { try? output.close() }
        let process = Process()
        process.executableURL = executable
        process.arguments = arguments
        process.standardInput = FileHandle.nullDevice
        process.standardOutput = output; process.standardError = output
        process.environment = ProcessInfo.processInfo.environment.filter { !$0.key.hasPrefix("DYLD_") }
        try process.run()
        let deadline = ProcessInfo.processInfo.systemUptime + timeout
        while process.isRunning && ProcessInfo.processInfo.systemUptime < deadline { Thread.sleep(forTimeInterval: 0.02) }
        if process.isRunning {
            kill(process.processIdentifier, SIGKILL); process.waitUntilExit()
            throw InstallError.message("The host compatibility check timed out. Nothing was changed. Reinstall the selected radio app and retry.")
        }
        process.waitUntilExit()
        guard process.terminationReason == .exit, process.terminationStatus == 0 else {
            let input = try FileHandle(forReadingFrom: log)
            defer { try? input.close() }
            let detail = String(decoding: try input.read(upToCount: 8192) ?? Data(), as: UTF8.self)
            throw InstallError.message("Host compatibility check failed. Nothing was changed. Reinstall the selected radio app or use a matching plugin build.\n" + detail)
        }
    }

    static func validate(payload: URL, host: URL, names: [String]) throws {
        let probe = payload.appendingPathComponent("dx_host_probe")
        guard FileManager.default.isExecutableFile(atPath: probe.path) else {
            throw InstallError.message("The compatibility checker is missing or not executable. Download the complete package again.")
        }
        try run(probe, arguments: [host.appendingPathComponent("Contents/MacOS/sdrpp").path,
            host.appendingPathComponent("Contents/Frameworks/libsdrpp_core.dylib").path,
            payload.appendingPathComponent("dx_notebook.dylib").path])
        for name in ["dx_decoder", "dx_modes_decoder", "dx_wspr_decoder", "dx_js8_decoder"] where names.contains(name) {
            // Exit after loading, before decoder initialization or receiver work.
            try run(payload.appendingPathComponent(name), arguments: ["--check-runtime"])
        }
    }
}
