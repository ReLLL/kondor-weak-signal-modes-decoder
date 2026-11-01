# Installation

Read when: installing, upgrading, removing or restoring the plugin.

## macOS native installer

Requirements: Apple Silicon and macOS 15+, with **SDR++** at `/Applications/SDR++.app` or **SDR++Brown** at `/Applications/SDR++Brown.app`. The installer does not restrict installation by version number or an exact core-library fingerprint. It checks the selected application and the libraries required by its plugin build. Standard SDR++ does not require Brown. Python, Homebrew and developer tools are not required for the native installer. Testing used SDR++ 1.2.0 and SDR++Brown 1.2.1 on macOS 26.

1. Install either supported radio application in Applications.
2. Open the release DMG, or extract the ZIP, and open **Install Kondor Weak Signal Modes Decoder**.
3. The installer detects an installed host. If both are installed, select **SDR++** or **SDR++Brown**. An existing **Home locator** is prefilled. With an empty field, macOS Location Services can suggest one after permission. Check the location estimate before installing, or click **Find grid on map**. It can also be set later.
4. Click **Install**. Missing first-run host settings are prepared automatically. A running host is asked to quit normally; the installer waits for its settings and never force-quits it.
5. Click **Open SDR++** or **Open SDR++Brown**, then open the plugin from the sidebar.

## Home location

New to grid locators? Follow [Finding your home grid](../README.md#finding-your-home-grid-first-time-setup). Automatic suggestions use macOS Location Services, with accuracy shown and confirmation through Install. Declining permission leaves manual entry and the locator map available. Existing values are retained; IP geolocation is not used.

The installer's optional **Home locator** field accepts the receiver's Maidenhead grid. Brown accepts four or six characters; standard SDR++ accepts four, six or eight. The installer saves it in the appropriate host setting. It preserves an existing locator if the field is left empty.

To change it later, use **Source > My Grid** in Brown's left sidebar, below **Operator Callsign**. Brown displays coordinates beside a valid grid and **Invalid** otherwise. For standard SDR++, use the plugin's **Settings > General > Maidenhead grid**. Standard SDR++ also has an optional receiver callsign in that settings page for the GridTracker context.

Use your actual receiving location. Distances and bearings use the center of the Maidenhead area, so a longer locator usually gives a closer estimate. A callsign-derived country does not provide an exact station position. With no valid home locator, standard SDR++ still decodes and maps received station grids, but does not invent a home marker, distance or bearing.

Installation is per user and requires no administrator password. Default profiles: `~/Library/Application Support/sdrpp-brown/` for Brown and `~/Library/Application Support/sdrpp/` for standard SDR++. The installer copies the plugin, helper and world geometry into `plugins/dx-notebook/` and registers the module. It verifies payload checksums, the selected app identity and required host libraries before writing. Version numbers and rebuilt-core fingerprints do not block installation.

The package includes its matching VOLK and single-precision FFTW runtimes under private Kondor filenames. Host upgrades such as VOLK 3.1 to 3.3 therefore need no manual library repair. Install the complete package when upgrading: its runtime libraries, notices and decoder helpers form one verified payload. Rerunning the installer restores missing or damaged plugin files while keeping settings and reception history.

Before changing the profile, the installer checks the selected host's Apple Silicon architecture, loads its core and the plugin in a separate process, checks required symbols and the ImGui layout, and uses each decoder helper's runtime-check option to verify its dependencies. Failures identify the missing library or incompatible symbol where available. The installer stays responsive during these checks. An incomplete radio app needs a complete reinstall of that app; a changed host ABI needs a matching plugin build. Keep the reported diagnostics when asking for support. See [compatibility validation](HOST_COMPATIBILITY.md) for the test matrix and limits.

Settings and reception logs stay in place. A timestamped backup of the original configuration and previous plugin goes under `dx-notebook-backups/`. Copied files are verified before replacement, configuration replacement is atomic, and a handled installation failure restores the previous plugin/configuration. The installer also checks that the selected host has not restarted or changed the configuration before committing changes. Do not launch the selected host during installation.

Kondor upgrades rename earlier sidebar entries while preserving their receiver grid, callsign and enabled state. Existing module IDs, data directories and helper filenames remain stable so reception history and preferences carry forward.

## Custom profile or upgrade

Choose **Choose profile...** and select the selected host's profile folder containing `config.json`. Upgrade by running the same installer; it saves another backup and preserves existing preferences and logs, including a disabled plugin's enabled/disabled setting. The host begins with clear visible views on its next launch and never loads previous sessions into those views.

For advanced users who already have Python 3, the previous command remains available in the extracted package:

```sh
python3 scripts/install.py --build bin --profile '/path/to/brown/profile'
```

Omit `--profile` for Brown's normal profile. For standard SDR++, use `python3 scripts/install.py --host sdrpp --build bin-sdrpp`; the matching host manifest and country data are included. Python is needed only for these optional commands and source-build tooling.

## Manual macOS installation

Use the binary macOS release ZIP. Open the selected radio app once, then quit it completely before copying files or editing settings. Back up the profile's `config.json` and any existing `plugins/dx-notebook/` folder first. Keep SDR++ at `/Applications/SDR++.app` or Brown at `/Applications/SDR++Brown.app`, matching the packaged libraries.

| Host | Profile folder | Binary folder in ZIP |
| --- | --- | --- |
| SDR++Brown | `~/Library/Application Support/sdrpp-brown/` | `bin/` |
| SDR++ | `~/Library/Application Support/sdrpp/` | `bin-sdrpp/` |

Finder's **Go > Go to Folder...** accepts these paths. For a custom profile, use the folder containing that host's `config.json`.

1. Create `plugins/dx-notebook/` inside the profile. Copy **every file** from the matching binary folder into it, including decoder executables, runtime libraries and license/provenance files.
2. Copy `resources/world.geojson` from the ZIP into the same destination. For standard SDR++, also copy `resources/cty.dat` and `resources/cty-LICENSE.txt` there. Keep the two host builds separate.
3. Open the profile's `config.json` in a plain-text editor. Merge the entries below into its existing `modules`, `moduleInstances` and `menuElements` fields. Preserve all other modules, instances and menu entries. For an upgrade, retain the existing decoder instance's settings instead of creating a duplicate.

This example uses Brown's default profile. Replace `YOUR_MAC_USERNAME` with your Mac account's home-folder name; for standard SDR++, replace `sdrpp-brown` with `sdrpp`. Use the full absolute path: JSON does not expand `~` or shell variables.

```json
{
  "modules": [
    "/Users/YOUR_MAC_USERNAME/Library/Application Support/sdrpp-brown/plugins/dx-notebook/dx_notebook.dylib"
  ],
  "moduleInstances": {
    "Kondor Weak Signal Modes Decoder": {
      "module": "advanced_ft8_ft4_dx_decoder",
      "enabled": true
    }
  },
  "menuElements": [
    { "name": "Kondor Weak Signal Modes Decoder", "open": true }
  ]
}
```

This is an example of the entries to merge, not a replacement for your whole configuration. Save valid JSON with commas between existing entries. Helpers must retain executable permission. If your extraction tool removed it, run the following in Terminal for Brown, or change `sdrpp-brown` to `sdrpp` for standard SDR++:

```sh
chmod u+x "$HOME/Library/Application Support/sdrpp-brown/plugins/dx-notebook/"dx*decoder
```

Reopen the host, enable/open the decoder from the sidebar, and [set your home grid](../README.md#finding-your-home-grid-first-time-setup). These files belong in the profile, outside the signed radio `.app`. Manual installation skips the installer's file verification, host checks and automatic backups.

## Windows installation: SDR#

Use the Windows package for the x64 .NET 9 SDR# host. Close SDR#, then choose either method:

- **Installer:** extract the complete ZIP, open `Install.cmd` and select `SDRSharp.dotnet9.exe`.
- **Manual copy:** copy the ZIP's top-level `AdvancedDx` folder into the `Plugins` folder beside that executable. The adapter must be at `Plugins\AdvancedDx\SDRSharp.AdvancedDx.dll`; its worker must be at `Plugins\AdvancedDx\AdvancedDx\dx_sdrsharp.exe`. Retain all nested runtime DLLs, helpers, maps and icons. If upgrading, move the previous `AdvancedDx` folder to a backup outside `Plugins` first.

If `SDRSharp.config` has a custom `core.pluginsDirectory`, use that directory instead of `Plugins`. SDR# discovers the adapter automatically; no `Plugins.xml` entry is required. Reopen SDR#, enable the decoder in the plugins panel, open its window and set **Settings > General > Maidenhead grid**.

Preferences, SQLite history, reception CSV and map caches are separate, under `%LOCALAPPDATA%\Advanced DX Decoder\SDRSharp`; leave them in place when replacing plugin binaries. See [Windows support and build details](WINDOWS.md).

## Remove or restore on macOS

Quit the selected host. Copy a chosen backup's `config.json` to the profile's `config.json`. If the backup contains `plugin/`, copy its files back into `plugins/dx-notebook/` to restore the previous plugin build. The installer's **Show backup** button opens the backup for that installation.

For a first installation, restoring the original configuration removes the registration. The unused plugin directory can be moved to Trash. Reception history and CSV logs are stored separately; remove them only if you intend to erase those logs.

## Troubleshooting

- **First-run setup needed:** open the selected host once and return to the installer. Select a custom profile if you normally use one.
- **Missing host library:** reinstall the complete host application in Applications; the plugin needs the libraries supplied by that host.
- **The radio application could not quit:** quit it normally and click Retry; it may be waiting for a dialog. The installer never force-quits it.
- **File integrity check failed:** download and extract the complete package again.
- **No plugin window:** check the sidebar/module registration and matching host build.
- **No messages:** start the SDR, check modes and eligible dial frequencies, wait for complete UTC slots, and check clock synchronization and audio levels.
- **Unknown distance:** the station has no recent received grid. Country lookup alone cannot establish distance.
- **Missing tiles:** check connectivity or select the offline map. Viewed tiles remain available within cache limits.
- **CSV error:** choose a writable path and compatible CSV file; the logger refuses unrelated file headers.

Use GitHub Issues for reproducible problems. Include versions and steps; review logs and screenshots for private information before sharing.
