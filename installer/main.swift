import AppKit
import Foundation

func hostIsRunning(_ manifest: PayloadManifest) throws -> Bool {
    if NSRunningApplication.runningApplications(withBundleIdentifier: manifest.hostBundleID).contains(where: { !$0.isTerminated }) { return true }
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/pgrep")
    process.arguments = ["-f", NSRegularExpression.escapedPattern(for: manifest.appPath + "/Contents/MacOS/sdrpp")]
    process.standardOutput = Pipe(); process.standardError = Pipe()
    try process.run(); process.waitUntilExit()
    guard [0, 1].contains(process.terminationStatus) else {
        throw InstallError.message("Unable to check whether the radio application is running. Nothing was changed.")
    }
    return process.terminationStatus == 0
}

final class InstallerWindow: NSObject, NSApplicationDelegate, NSWindowDelegate, NSTextFieldDelegate {
    private var window: NSWindow!
    private let status = NSTextField(wrappingLabelWithString: "")
    private let profileLabel = NSTextField(wrappingLabelWithString: "")
    private let primary = NSButton(title: "Install", target: nil, action: nil)
    private let choose = NSButton(title: "Choose profile...", target: nil, action: nil)
    private let cancel = NSButton(title: "Cancel", target: nil, action: nil)
    private let help = NSButton(title: "Installation help", target: nil, action: nil)
    private let diagnostics = NSButton(title: "Copy diagnostics", target: nil, action: nil)
    private var diagnosticText = ""
    private let progress = NSProgressIndicator()
    private let hosts = NSPopUpButton()
    private let homeGrid = NSTextField()
    private let locate = NSButton(title: "Use my location", target: nil, action: nil)
    private let gridMap = NSButton(title: "Find grid on map", target: nil, action: nil)
    private let locationStatus = NSTextField(wrappingLabelWithString: "")
    private let locationLookup = LocationLookup()
    private var automaticLocationAttempted = false
    private var payloads: [URL] = []
    private var manifests: [PayloadManifest] = []
    private var selectedHost = 0
    private var profile = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/sdrpp-brown")
    private var host: URL { URL(fileURLWithPath: manifests[selectedHost].appPath) }
    private var busy = false
    private var receipt: InstallReceipt?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        let menu = NSMenu()
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Quit Installer", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        let item = NSMenuItem(); item.submenu = appMenu; menu.addItem(item)
        NSApp.mainMenu = menu
        if let resources = Bundle.main.resourceURL {
            for name in ["Payload", "PayloadSDRPP"] {
                let path = resources.appendingPathComponent(name)
                if let data = try? Data(contentsOf: path.appendingPathComponent("manifest.json")),
                   let manifest = try? JSONDecoder().decode(PayloadManifest.self, from: data) {
                    payloads.append(path); manifests.append(manifest)
                }
            }
        }
        if let index = manifests.indices.first(where: { FileManager.default.fileExists(atPath: manifests[$0].appPath) }) { selectedHost = index }
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 550, height: 580),
            styleMask: [.titled, .closable], backing: .buffered, defer: false)
        window.title = "Install Kondor Weak Signal Modes Decoder"
        window.delegate = self
        window.center()
        let stack = NSStackView()
        stack.orientation = .vertical; stack.alignment = .leading; stack.spacing = 16
        stack.translatesAutoresizingMaskIntoConstraints = false
        window.contentView!.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: window.contentView!.leadingAnchor, constant: 28),
            stack.trailingAnchor.constraint(equalTo: window.contentView!.trailingAnchor, constant: -28),
            stack.topAnchor.constraint(equalTo: window.contentView!.topAnchor, constant: 24)
        ])
        let title = NSTextField(wrappingLabelWithString: "Kondor Weak Signal Modes Decoder: FT8 / FT4 / WSPR / FST4 / FST4W / JS8 / Q65")
        title.font = .boldSystemFont(ofSize: 22)
        stack.addArrangedSubview(title)
        let intro = NSTextField(wrappingLabelWithString: "Install for SDR++ or SDR++Brown. Choose your radio application below.")
        intro.font = .systemFont(ofSize: 13)
        stack.addArrangedSubview(intro)
        let safety = NSTextField(wrappingLabelWithString: "Your settings and reception logs stay in place. The installer saves a backup before making changes.")
        safety.textColor = .secondaryLabelColor; safety.font = .systemFont(ofSize: 12)
        stack.addArrangedSubview(safety)
        for manifest in manifests { hosts.addItem(withTitle: manifest.hostName) }
        hosts.selectItem(at: selectedHost); hosts.target = self; hosts.action = #selector(selectHost)
        if manifests.count > 1 { stack.addArrangedSubview(hosts) }
        let location = NSStackView(views: [NSTextField(labelWithString: "Home locator"), homeGrid])
        location.orientation = .horizontal; location.spacing = 12
        homeGrid.placeholderString = "Your grid, e.g. KM69xx (optional)"
        homeGrid.delegate = self
        homeGrid.widthAnchor.constraint(equalToConstant: 290).isActive = true
        stack.addArrangedSubview(location)
        let locationButtons = NSStackView(views: [locate, gridMap]); locationButtons.orientation = .horizontal; locationButtons.spacing = 12
        stack.addArrangedSubview(locationButtons)
        locationStatus.font = .systemFont(ofSize: 11); locationStatus.textColor = .secondaryLabelColor
        stack.addArrangedSubview(locationStatus)
        profileLabel.font = .systemFont(ofSize: 11); profileLabel.textColor = .secondaryLabelColor
        stack.addArrangedSubview(profileLabel)
        status.font = .systemFont(ofSize: 13); status.setAccessibilityIdentifier("installStatus")
        stack.addArrangedSubview(status)
        diagnostics.bezelStyle = .rounded; diagnostics.target = self; diagnostics.action = #selector(copyDiagnostics)
        diagnostics.isHidden = true; stack.addArrangedSubview(diagnostics)
        progress.style = .spinning; progress.controlSize = .small; progress.isDisplayedWhenStopped = false
        stack.addArrangedSubview(progress)
        let buttons = NSStackView(views: [help, NSView(), cancel, primary])
        buttons.orientation = .horizontal; buttons.spacing = 12
        buttons.translatesAutoresizingMaskIntoConstraints = false
        window.contentView!.addSubview(buttons)
        NSLayoutConstraint.activate([
            buttons.leadingAnchor.constraint(equalTo: stack.leadingAnchor),
            buttons.trailingAnchor.constraint(equalTo: stack.trailingAnchor),
            buttons.bottomAnchor.constraint(equalTo: window.contentView!.bottomAnchor, constant: -24)
        ])
        for button in [primary, choose, cancel, help, locate, gridMap] { button.bezelStyle = .rounded; button.target = self }
        locate.action = #selector(findLocation); gridMap.action = #selector(openGridMap)
        primary.action = #selector(install); primary.keyEquivalent = "\r"
        primary.setAccessibilityIdentifier("installButton")
        cancel.action = #selector(close); cancel.keyEquivalent = "\u{1b}"
        choose.action = #selector(chooseProfile)
        help.action = #selector(openHelp)
        stack.addArrangedSubview(choose)
        selectHost()
        window.makeKeyAndOrderFront(nil); NSApp.activate(ignoringOtherApps: true)
    }

    private func installer() throws -> PluginInstaller {
        guard manifests.indices.contains(selectedHost) else { throw InstallError.message("The installer package is incomplete.") }
        let manifest = manifests[selectedHost]
        return try PluginInstaller(payload: payloads[selectedHost], brownRunning: { try hostIsRunning(manifest) })
    }

    private func refresh() {
        diagnostics.isHidden = true
        let folder = manifests.indices.contains(selectedHost) ? manifests[selectedHost].profileFolder : "sdrpp-brown"
        profileLabel.stringValue = profile == FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/" + folder)
            ? "Install for the current macOS user in the normal profile."
            : "Selected profile: " + profile.lastPathComponent
        let engine: PluginInstaller
        do { engine = try installer() } catch {
            showDiagnostic(error); primary.isEnabled = false; return
        }
        let index = selectedHost, checkedProfile = profile, checkedHost = host
        primary.isEnabled = false; status.stringValue = "Checking radio compatibility..."
        DispatchQueue.global(qos: .userInitiated).async {
            let result: Result<(String, String), Error>
            do {
                try engine.validatePayload(); try engine.validateHost(checkedHost)
                let exists = FileManager.default.fileExists(atPath: checkedProfile.appendingPathComponent("config.json").path)
                if !exists { result = .success(("Install will prepare \(engine.manifest.hostName)'s settings automatically.", "Install")) }
                else if try hostIsRunning(engine.manifest) { result = .success(("\(engine.manifest.hostName) is open. Install will ask it to quit safely first.", "Quit and Install")) }
                else { result = .success(("Ready to install for \(engine.manifest.hostName).", "Install")) }
            } catch { result = .failure(error) }
            DispatchQueue.main.async {
                guard !self.busy, self.selectedHost == index, self.profile == checkedProfile else { return }
                switch result {
                case .success(let (message, title)):
                    self.status.stringValue = message; self.primary.title = title; self.primary.isEnabled = true; self.primary.action = #selector(self.install)
                case .failure(let error):
                    self.showDiagnostic(error); self.primary.title = "Retry"; self.primary.action = #selector(self.recheck); self.primary.isEnabled = true
                }
            }
        }
    }

    @objc private func install() {
        if let installed = receipt {
            let configuration = NSWorkspace.OpenConfiguration()
            configuration.arguments = ["-r", installed.profile.path]
            primary.isEnabled = false
            NSWorkspace.shared.openApplication(at: host, configuration: configuration) { _, error in
                DispatchQueue.main.async {
                    if let error { self.failed(error) } else { NSApp.terminate(nil) }
                }
            }
            return
        }
        let location = homeGrid.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        let selectedProfile = profile, selectedApp = host
        let engine: PluginInstaller
        do { engine = try installer(); try engine.validateHomeGrid(location) } catch { failed(error); return }
        locationLookup.cancel(); locate.isEnabled = false; gridMap.isEnabled = false
        busy = true; primary.isEnabled = false; choose.isEnabled = false; cancel.isEnabled = false; hosts.isEnabled = false; homeGrid.isEnabled = false
        progress.startAnimation(nil); status.stringValue = "Preparing installation..."
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let config = selectedProfile.appendingPathComponent("config.json")
                if !FileManager.default.fileExists(atPath: config.path) {
                    _ = DispatchQueue.main.sync { NSWorkspace.shared.open(selectedApp) }
                    for _ in 0..<300 {
                        if FileManager.default.fileExists(atPath: config.path) { break }
                        Thread.sleep(forTimeInterval: 0.1)
                    }
                    guard FileManager.default.fileExists(atPath: config.path) else { throw InstallError.message("The radio application needs first-run setup. Open it once, then retry Install.") }
                }
                let apps = NSRunningApplication.runningApplications(withBundleIdentifier: engine.manifest.hostBundleID)
                for app in apps where !app.isTerminated {
                    guard app.terminate() else { throw InstallError.message("The radio application could not quit automatically. Quit it and try again.") }
                }
                if !apps.isEmpty {
                    for _ in 0..<100 {
                        if try !hostIsRunning(engine.manifest) { break }
                        Thread.sleep(forTimeInterval: 0.1)
                    }
                }
                let installed = try engine.install(profile: selectedProfile, host: selectedApp, homeGrid: location)
                DispatchQueue.main.async { self.finished(installed) }
            } catch { DispatchQueue.main.async { self.failed(error) } }
        }
    }

    private func finished(_ installed: InstallReceipt) {
        receipt = installed; busy = false; progress.stopAnimation(nil)
        status.stringValue = "Installed. Open \(manifests[selectedHost].hostName) and find Kondor Weak Signal Modes Decoder in the sidebar."
        status.textColor = .systemGreen
        primary.title = "Open \(manifests[selectedHost].hostName)"; primary.isEnabled = true
        cancel.title = "Done"; cancel.isEnabled = true
        choose.title = "Show backup"; choose.isEnabled = true; choose.action = #selector(showBackup)
    }

    private func failed(_ error: Error) {
        busy = false; progress.stopAnimation(nil)
        showDiagnostic(error); status.textColor = .systemRed
        primary.title = "Retry"; primary.action = #selector(recheck); primary.isEnabled = true; cancel.isEnabled = true; choose.isEnabled = true; hosts.isEnabled = true; homeGrid.isEnabled = true
        locate.isEnabled = true; gridMap.isEnabled = true
    }

    private func showDiagnostic(_ error: Error) {
        status.stringValue = error.localizedDescription.components(separatedBy: .newlines).first ?? error.localizedDescription
        diagnosticText = error.localizedDescription
        if manifests.indices.contains(selectedHost) {
            let info = NSDictionary(contentsOf: host.appendingPathComponent("Contents/Info.plist"))
            diagnosticText = "Kondor \(manifests[selectedHost].version); \(manifests[selectedHost].hostName) \(info?["CFBundleVersion"] as? String ?? "unknown version")\n" + diagnosticText
        }
        diagnostics.isHidden = false; status.toolTip = diagnosticText
    }
    @objc private func recheck() { status.textColor = .labelColor; refresh() }
    @objc private func copyDiagnostics() { NSPasteboard.general.clearContents(); NSPasteboard.general.setString(diagnosticText, forType: .string) }

    @objc private func chooseProfile() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.allowsMultipleSelection = false
        panel.message = "Choose the radio application's profile folder containing config.json."
        if panel.runModal() == .OK, let url = panel.url {
            guard FileManager.default.fileExists(atPath: url.appendingPathComponent("config.json").path) else {
                failed(InstallError.message("Choose a profile folder containing config.json.")); return
            }
            profile = url; loadHomeGrid(); status.textColor = .labelColor; refresh()
        }
    }
    @objc private func showBackup() { if let receipt { NSWorkspace.shared.open(receipt.backup) } }
    @objc private func selectHost() {
        selectedHost = hosts.indexOfSelectedItem
        guard manifests.indices.contains(selectedHost) else { refresh(); return }
        profile = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/" + manifests[selectedHost].profileFolder)
        loadHomeGrid()
        status.textColor = .labelColor; refresh()
    }
    private func loadHomeGrid() {
        locationLookup.cancel(); locate.isEnabled = true
        homeGrid.stringValue = ""
        if let data = try? Data(contentsOf: profile.appendingPathComponent("config.json")),
           let config = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] {
            if manifests[selectedHost].profileFolder == "sdrpp" {
                homeGrid.stringValue = (config["moduleInstances"] as? [String: [String: Any]])?[PluginInstaller.instance]?["receiverGrid"] as? String ?? ""
            } else { homeGrid.stringValue = config["operatorLocation"] as? String ?? "" }
        }
        locationStatus.stringValue = homeGrid.stringValue.isEmpty ? "A grid is a short location code. Use your receiver's location, not a callsign." : "Existing home grid retained. Change it only if your receiver has moved."
        if homeGrid.stringValue.isEmpty && !automaticLocationAttempted {
            automaticLocationAttempted = true
            DispatchQueue.main.async { if self.homeGrid.stringValue.isEmpty && !self.busy { self.findLocation() } }
        }
    }
    @objc private func findLocation() {
        locate.isEnabled = false; locationStatus.stringValue = "Finding your location... Allow macOS location access to suggest a grid."
        let original = homeGrid.stringValue
        locationLookup.start { [weak self] grid, accuracy, message in
            guard let self, !self.busy, self.homeGrid.stringValue == original else { return }
            self.locate.isEnabled = true
            if let grid, let accuracy {
                self.homeGrid.stringValue = grid
                self.locationStatus.stringValue = String(format: "Suggested %@ (accuracy about %.1f km). %@", grid, accuracy / 1000, message)
            } else { self.locationStatus.stringValue = message }
        }
    }
    @objc private func openGridMap() { NSWorkspace.shared.open(URL(string: "https://k7fry.com/grid/")!) }
    func controlTextDidChange(_ notification: Notification) { locationLookup.cancel(); locate.isEnabled = true; locationStatus.stringValue = "Use the grid of the place where your receiver and antenna are located." }
    @objc private func openHelp() { NSWorkspace.shared.open(URL(string: "https://github.com/ReLLL/kondor-weak-signal-modes-decoder/blob/main/docs/INSTALL.md")!) }
    @objc private func close() { if !busy { NSApp.terminate(nil) } }
    func windowShouldClose(_ sender: NSWindow) -> Bool { if busy { return false }; NSApp.terminate(nil); return true }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply { busy ? .terminateCancel : .terminateNow }
}

let delegate = InstallerWindow()
NSApplication.shared.delegate = delegate
NSApplication.shared.run()
