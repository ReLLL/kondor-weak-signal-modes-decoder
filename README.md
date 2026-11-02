# Kondor Weak Signal Modes Decoder: FT8 / FT4 / WSPR / FST4 / FST4W / JS8 / Q65

**Receive-only FT8, FT4, WSPR, FST4/FST4W, Q65 and JS8 decoding, a DX map, and reception logging for SDR++, SDR++Brown and SDR# (SDR Sharp).** FT8, FT4 and WSPR start on; enable the other modes in Settings. See the stations you hear, their countries, receiver SNR, approximate distances and recent exchanges without opening a separate mapper.

Created by [ReLLL](https://github.com/ReLLL). Free software under GPL-3.0-or-later.

## Credits

This plugin builds on the weak-signal protocols, decoder algorithms and software developed by the **WSJT-X / WSJT development team**, including **Joe Taylor K1JT, Steve Franke K9AN, Bill Somerville G4WJS and Nico Palermo IV3NWV**, and many other contributors. Our FT8/FT4 decoder comes through **MSHV**, adapted by **Hrisimir Hristov LZ2HV**, and SDR++Brown. We thank the protocol designers, decoder developers, maintainers and testers whose work makes this possible.

ReLLL develops this plugin's receiver integration, mapping, reception history and interface. See [full credits](CREDITS.md) for upstream projects, authors and source references, and [third-party notices](THIRD_PARTY_NOTICES.md) for licenses. Upstream credits do not imply endorsement.

FT8/FT4 reception tables and details show decoder confidence. Questionable decodes remain in history for inspection but do not establish map spots, distance records or DX activity bearings. See [confidence and reception reliability](docs/DECODER_RELIABILITY.md).

## Download and install

Version **1.0.67** adds Linux source installation, confidence-aware FT8/FT4 reception and live social-sharing images. It bundles matching runtimes, checks host compatibility and starts with the decoder window hidden. Earlier packages use **Advanced FT8/FT4/WSPR DX Decoder**; existing settings and reception history are preserved when updating.

Choose your operating system, then follow its steps below. Each download contains the files for that platform.

| Operating system | SDR application | Start here |
| --- | --- | --- |
| macOS | SDR++ or SDR++Brown | [Download DMG](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/download/v1.0.67/kondor-weak-signal-modes-decoder-1.0.67-installer-macos-arm64.dmg) · [Install steps](#macos-sdr-and-sdrbrown) |
| Windows | SDR# / SDR Sharp | [Download preview ZIP](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/download/v1.0.67/kondor-weak-signal-modes-decoder-1.0.67-sdrsharp-windows-x64-preview.zip) · [Install steps](#windows-sdr--sdr-sharp) |
| Linux | SDR++ | [Download source installer](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/download/v1.0.67/kondor-weak-signal-modes-decoder-1.0.67-linux-source-installer.tar.gz) · [Install steps](#linux-native-source-installer) |

**Before receiving, enable automatic clock synchronization.** These modes use timed reception windows; an inaccurate clock can prevent decoding even when signals are visible. See [clock setup and troubleshooting](#clock-synchronization-before-receiving).

### macOS: SDR++ and SDR++Brown

**Requirements:** Apple Silicon Mac, macOS 15 or later, and SDR++ or SDR++Brown. Verified on macOS 26 with SDR++ 1.2.0 (May 28, 2024) and SDR++Brown 1.2.1 (August 4, 2026).

**Recommended download:** [macOS installer DMG](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/download/v1.0.67/kondor-weak-signal-modes-decoder-1.0.67-installer-macos-arm64.dmg). Developer ID signed, notarized and stapled.

1. Install [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus/releases/tag/nightly) or [SDR++Brown](https://sdrpp-brown.san.systems/) in **Applications**. Skip this if you already have the supported app installed.
2. Download and open `installer-macos-arm64.dmg`, then open **Install Kondor Weak Signal Modes Decoder**. The ZIP includes the same installer if you prefer to extract it.
3. Choose your radio app. Check the prefilled **Home locator**, or use **Find grid on map**. An empty locator can also receive a suggestion through macOS Location Services. Click **Install**; the installer prepares the profile and asks an open host to quit safely if needed.
4. Click **Open SDR++** or **Open SDR++Brown**. Open **Kondor Weak Signal Modes Decoder** from the sidebar.
5. If you skipped the locator, set it before using distances or bearings. In **Brown**, expand **Source > My Grid**, below **Operator Callsign**. In **SDR++**, use Kondor's **Settings > General > Maidenhead grid**. [Find your own six-character grid](#finding-your-home-grid-first-time-setup); do not copy an example grid.
6. Start your SDR and tune a capture covering a supported FT8, FT4 or WSPR dial frequency. Eligible presets show **Receiving**.

No Python, Terminal, Homebrew, developer tools or administrator password is needed for the native installer. It verifies packaged files and the selected host's required libraries, backs up the configuration and any previous plugin, and installs outside the host's signed application. Each host has its own binary, profile and reception history. Settings and logs are preserved; **Choose profile...** supports a non-default profile. [Installation and removal](docs/INSTALL.md).

The **SDR++ build uses standard SDR++ APIs**, with its own location and country data; it does not require Brown to be installed. Brown keeps its native integration and existing behavior. The shared FT8/FT4 decoder uses pinned MSHV-derived source, while host headers and runtime libraries match the selected application.

#### Manual macOS installation: copy files yourself

Use the macOS release ZIP without opening its installer. Open your radio app once to create its profile, then quit it. In Finder, **Go > Go to Folder...** opens the folders below; create `plugins/dx-notebook/` if needed.

| Radio app | Copy every file from the ZIP folder | Destination |
| --- | --- | --- |
| SDR++Brown | `bin/` | `~/Library/Application Support/sdrpp-brown/plugins/dx-notebook/` |
| SDR++ | `bin-sdrpp/` | `~/Library/Application Support/sdrpp/plugins/dx-notebook/` |

Also copy `resources/world.geojson` into that destination. For standard SDR++, copy `resources/cty.dat` and `resources/cty-LICENSE.txt` there too. Keep all helpers, runtime libraries and license files together; use the build for the selected radio app.

In the profile's `config.json`, register the absolute path to `dx_notebook.dylib` in `modules`, add its enabled instance in `moduleInstances`, and add its sidebar entry in `menuElements`. **Preserve existing entries.** The [manual installation guide](docs/INSTALL.md#manual-macos-installation) contains the exact JSON entries and executable-permission command. Then reopen the radio app and set your home grid as above. Custom profiles use the same layout inside their own profile folder; files go outside the signed `.app` bundle.

### Windows: SDR# / SDR Sharp

**Requirements:** the **x64 .NET 9** edition of SDR# from Airspy. Validated with SDR# 1.0.0.1921 on Windows 11 ARM64 using x64 emulation, including recorded FT8 reception, close/reopen and worker recovery. All seven modes pass Windows decoder fixtures. This preview does not target x86 or .NET 8 SDR#.

[Download the Windows preview ZIP](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/download/v1.0.67/kondor-weak-signal-modes-decoder-1.0.67-sdrsharp-windows-x64-preview.zip). Installation takes these steps:

1. Install the **x64 .NET 9** version of [SDR# from Airspy](https://airspy.com/download/) and check that SDR# opens normally.
2. Extract the complete Windows decoder ZIP, then close SDR#.
3. Open **Install.cmd** and select **SDRSharp.dotnet9.exe** in your SDR# folder. The installer verifies the package, installs the plugin and backs up an existing installation.
4. Open SDR#. Find **Kondor Weak Signal Modes Decoder** in the plugins panel, enable it, and click **Open Kondor Decoder**.
5. In the decoder, set **Settings > General > Maidenhead grid** to your receiver's location. Find the locator with the [K7FRY grid map](https://k7fry.com/grid/): click your receiver's location and copy its first six characters.
6. Start the radio and tune the captured spectrum to a supported dial frequency. FT8, FT4 and WSPR are enabled initially. Closing the decoder window keeps reception running; its Open button brings it back.

Windows settings, reception CSV, SQLite history and cached maps are stored under `%LOCALAPPDATA%\Advanced DX Decoder\SDRSharp`. Updates preserve these files. See the [Windows guide](docs/WINDOWS.md) for build steps, installation details and validation limits.

#### Manual Windows installation: copy files yourself

1. Close SDR# and extract the complete Windows decoder ZIP.
2. Copy the ZIP's **top-level `AdvancedDx` folder** into the `Plugins` folder beside `SDRSharp.dotnet9.exe`. The result includes `Plugins\AdvancedDx\SDRSharp.AdvancedDx.dll` and `Plugins\AdvancedDx\AdvancedDx\dx_sdrsharp.exe`. Keep the nested folder and all its DLLs, helpers, maps and icons together.
3. If upgrading, move the previous `Plugins\AdvancedDx` folder to a backup **outside `Plugins`** before copying. If `SDRSharp.config` sets `core.pluginsDirectory` to another location, use that plugins folder instead.
4. Reopen SDR#, enable the decoder in its plugins panel and set your home grid as above. No `Plugins.xml` entry is needed. Reception history and preferences remain in the separate data folder above.

### Linux: native source installer

**Requirements:** a Linux desktop, Internet access, Git and Python 3. The installer builds natively for your CPU and installs the remaining build dependencies. Ubuntu 24.04 ARM64 and emulated x86-64 were validated in containers. Fedora/Arch recipes and physical Linux RF reception still need community testing.

1. Download the [Linux source-installer archive](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/download/v1.0.67/kondor-weak-signal-modes-decoder-1.0.67-linux-source-installer.tar.gz).
2. Extract it. Open a terminal in the extracted folder and run:

   ```sh
   bash install-linux.sh --install-deps
   ```

3. Let the build finish. Only dependency installation requests `sudo`; run the command as your normal user.
4. Open **SDR++ with Kondor** from your application menu. Open Kondor from the sidebar, set **Settings > General > Maidenhead grid**, and start your receiver.

The default installer builds a matching SDR++ alongside Kondor in your user account. Existing radio apps, profiles and reception files stay in place. The first build takes longer; later builds reuse the cache. Use `--jobs 2` if memory is limited.

#### Existing host or manual Linux installation

To install into an existing SDR++, supply its matching source, core library, executable and profile. The [Linux guide](docs/LINUX.md#existing-sdr-installation) gives the command and common folder locations. It also covers [manual compilation and file placement](docs/LINUX.md#manual-build-and-installation), cloning instead of downloading, backups and troubleshooting. Linux requires its own native build; use the Linux archive rather than macOS or Windows binaries.

### Finding your home grid: first-time setup

A **Maidenhead grid locator** is a short code for a place on Earth, such as `FN31pr`. It is not a callsign, postcode or frequency. Use the location of **your receiver and antenna**, not the grid of a station you hear. [ARRL explains grid squares](https://www.arrl.org/grid-squares).

The installer suggests a grid when the field is empty, using **macOS Location Services** after permission. It shows the estimated accuracy; check the suggestion before clicking Install. Existing grids stay unchanged. If permission is declined or the estimate is too coarse, installation still works and you can enter the grid later. IP addresses are not used to guess a home grid.

To find it yourself:

1. Open the [K7FRY locator map](https://k7fry.com/grid/) in your browser, or click **Find grid on map** in the installer.
2. Zoom and move the map to the place where your receiver is located. Click that spot; the map displays its locator.
3. Copy the **first six characters**. If the map shows a longer locator, keep only those six; six characters work in both hosts. Letter case does not matter.
4. Paste them into the installer's **Home locator** field. Already installed? In Brown, use **Source > My Grid** on the left. In standard SDR++, use the plugin's **Settings > General > Maidenhead grid**.
5. Check that the home marker is near your receiving location. If you move the receiver to another place, update its locator.

The code identifies an area; the plugin calculates distances from its center. Six characters give a smaller area than four. `KM69` and `KM69xx` in the instructions are format examples; do not copy them as your location unless they actually match it.

### Clock synchronization: before receiving

Most supported digital weak-signal modes rely on strict UTC time frames. Keep your computer clock synchronized through **NTP (Network Time Protocol)** or another accurate reference. Aim for an error of **less than half a second**; this is a practical target, not a universal decoder cutoff. The [WSJT-X guide](https://wsjt.sourceforge.io/wsjtx-main_en.html) specifies clock accuracy within **±1 second**. Your clock can display local time; you do not need to change its time zone to UTC.

| System | Enable automatic synchronization |
| --- | --- |
| **macOS** | Open **Apple menu > System Settings > General > Date & Time**. Enable **Set time and date automatically**. Under **Source / Set**, keep Apple's time server or your network's trusted NTP server. Select the correct time zone, automatically or manually. [Apple instructions](https://support.apple.com/guide/mac-help/mchlp2996/mac). |
| **Windows** | Open **Start > Settings > Time & language > Date & time**. Enable **Set time automatically**, choose the correct time zone, then click **Sync now** under **Additional settings / Synchronize your clock**. Check that synchronization succeeds. Labels vary by Windows version. [Microsoft instructions](https://support.microsoft.com/en-us/windows/experience/personalization/set-time-date-and-time-zone-settings-in-windows). |

**If signals are visible but few or none decode:**

- Check synchronization again after sleep, a long offline period or resuming a virtual machine. Automatic sync being enabled does not prove the clock is currently accurate. For optional Windows diagnostics, open **Terminal as administrator**: `w32tm /query /status` shows the last successful synchronization; `w32tm /stripchart /computer:time.windows.com /dataonly /samples:5` checks the offset against that server. [Windows time diagnostics](https://learn.microsoft.com/en-us/windows-server/networking/windows-time-service/windows-time-service-tools-and-settings).
- If sync fails, check connectivity and time-server access. A VPN, firewall or managed network may block NTP traffic on **UDP port 123**. Ask the network administrator for an approved time source; offline operation needs another accurate reference, such as GPS time.
- A similar DT offset across many stations can suggest a local timing problem, but **DT is not a measurement of your clock error alone**: transmitter timing and receiver/audio delays also contribute. Do not adjust the clock to match one station.
- Brown's **Source > Time correction** manually shifts capture timestamps; it does not synchronize the system clock. For live reception, normally leave it at **0 seconds** once the clock is correct, unless you deliberately need an offset.
- After correcting the clock, stop and restart reception and allow a **complete new sequence** to collect. Also check the dial frequency, selected mode and sequence settings; clock synchronization alone cannot fix every decoding problem.

## Features

- **At a glance:** unique countries, unique stations, farthest distance with country, active decoding uptime, stations heard in the last five minutes, and the latest three calls with country, distance and receiver SNR. DX ACTIVITY BEARING summarizes distant reception within the shared filters. With either saved-history option on, an amber clock value after uptime shows elapsed time from the oldest retained reception; hover for its start date and meaning.
- **Three reception views:** Unique stations, All messages and Countries heard. Repeated messages stay visible in All messages; country summaries show distinct senders, mean distance/SNR and the last five senders. Each table remembers its layout and sort.
- **Waterfall corner:** optional, initially on. Every new reception appears with its flag, country, sender, distance, SNR and full italic message, including repeats. Rows pulse for two seconds and expire after an adjustable 10 seconds. The farthest reception stays below, with a trophy and flag; only its Farthest label is gold.
- **Flags and details:** flags precede country names in all tables. The details pane separates station, country, distance, signal and message colors; receiver SNR follows the continuous map color scale. Drag its horizontal divider to adjust and remember its height. Double-click a country to see all its matching receptions, including repeats, newest first.
- **Distance units:** kilometers by default, with a saved miles option in General settings. Tables, map tips, statistics, details, notifications and distance filters follow this choice.
- **Flexible workspace:** resizable window, visible map/table splitter, sortable columns with saved drag order, widths and visibility, map-only and table-only views. Newest **Station first heard** comes first by default; repeats retain the original first-heard time.
- **Message detail:** callsign, country, grid, band, receive mode, SNR, approximate distance, message text, Msg age, Msg first heard and Station first heard. One row per callsign across bands and modes, with the latest matching message and receiver values. Age values show seconds with ago through 600 seconds, then date/time in the computer's time zone; sorting uses timestamps. Hover reveals local timestamps with the UTC offset; right-click opens Google, QRZ or HamQTH for the transmitting or addressed station.
- **DX map:** Esri street map by default, Humanitarian OpenStreetMap or bundled offline world map. Country names, day/night shading, home marker, continuous SNR coloring (default on, optional), faint country highlights, and dark-blue animated dotted arcs between addressed stations with received location evidence. Arcs follow the host's visible spectrum, including zoom and pan.
- **Focused reception:** one compact, responsive filter bar shared by the map and all table tabs, with band/mode filters, CQ/general-call and two-way message filters, search, exclusions and time windows. Sender, Target and Mod identify the transmitter, addressed callsign and latest receive mode; numeric ranges cover every signal, age and reception-count column.
- **Clear visual feedback:** nine-second reception highlight, latest-batch map halos and green age values, green country cells for the three most recently discovered countries, retained across message updates, and bold gold for the farthest station's country and distance.
- **Reversible false-decode filter:** optional Remove likely falses hides unresolved sender hashes and checks identified senders' received/cached grids against callsign-country outlines, with border tolerance and portable-station exceptions. It updates existing and new entries in views and reception statistics while preserving saved records. Valid portable/remote stations and unresolved hash receptions can also be hidden; this heuristic starts off.
- **Compact controls:** a fixed status bar shows fading operation messages without shifting the workspace. Default sorting restores the newest-first order; Restore defaults resets preferences and layouts after confirmation while preserving reception history and home setup.
- **Persistent logs:** UTF-8 CSV with UTC dates, callsigns, SNR, frequency, distance and message details. Commas, quotes and line breaks are escaped. Choose the file location and size limit in Settings.
- **Bounded database history:** the default keeps up to 100,000 saved receptions, then deletes the oldest in background batches until 95,000 remain. Checks run every 30 minutes and at startup. Settings > History > History DB shows record count, database size and retained start date, and lets you change both thresholds or disable the limit. CSV logs have a separate size limit. Saved statistics describe retained history. Larger limits can slow searches and filters.
- **Saved-message cleanup:** preview and confirm one-off deletion by local date range, country, missing distance or missing grid. SQLite and known CSV logs are cleaned together, map/table statistics update, and SQLite is compacted in the background. Delete transmitter and Ignore transmitter are separate actions, each with an optional checkbox to combine them.
- **Quick time windows:** choose Last hour, Last 6 hours, Last 24 hours, Last 7 days or Last month in the shared filter bar. All clears the time limit for both views; Reset clears reception filters too. Time windows remain available when saved history is enabled.
- **Optional GridTracker feed:** direct local UDP using WSJT-X packets, initially off, with `127.0.0.1:2237` ready to enable. No separate bridge is required for this feed.
- **Quiet defaults:** alert sounds, map marker counts and both saved-history options start off. Individual alert sounds are selectable.

The decoder window starts hidden each time the host launches. Enabled reception, logging and notifications continue in the background. Click Open Kondor Decoder in the host's plugin panel to show current data.

Closing the decoder window keeps reception, logging, alerts and integrations running while the sidebar switch is on. Open brings the window forward with current data and preserves its size and position. Each host starts with empty visible views and fresh session statistics by default. **Use saved history for stats** includes retained receptions in the totals. **Use saved history for everything** also restores the map, table and first-heard times. Both start off and load in the background; uptime always measures this session's active decoding. Live reception, table/map updates and logging continue while history is enabled. Clear asks for confirmation and preserves saved logs. Its checked **Reset stats** option returns to session mode and turns off both history options.

Map markers and table rows share station actions. Ignore transmitter blocks future receptions and keeps saved data. Delete transmitter removes saved receptions and allows new ones. Each confirmation offers one unchecked checkbox to also do the other operation. Unresolved and malformed identities are accepted; rows with no transmitter identity match only that exact message. Unignore allows future receptions without restoring deleted records.

## WSPR, FST4, FST4W and Q65 reception

Version 1.0.39 also receives **WSPR, FST4, FST4W and Q65**, using pinned official WSJT-X decoder routines. FT8, FT4 and WSPR start on, with switches together in the main window. FST4, FST4W and Q65 start off and can be enabled in **Modes...** or **Settings > Reception**. Saved on/off choices are remembered. **Sequence: All** is the default and checks every supported sequence length at once; choose one length to reduce decoder work. Q65 still needs matching tone spacing: All supports A-C; D requires 30 seconds or longer and E requires 60 seconds or longer. Q65 currently decodes individual sequences without cross-sequence averaging. FT8/FT4 keep their existing controls.

The same map, tables, countries, saved history and CSV logging handle the new receptions. The shared **Mod** filter selects a protocol; **Beacon** selects WSPR/FST4W messages. Details show reported transmitter power separately from the SNR measured at your receiver. Map shapes identify mode: circles for FT8/FT4, squares for WSPR, triangles for FST4, diamonds for FST4W and hexagons for Q65. Distances remain relative to your home grid; beacons have no QSO target or communication arc. GridTracker forwarding currently covers FT8/FT4.

Capture waits for complete UTC sequences. In All, the progress bar follows the shortest length while longer lengths collect in parallel; 30-minute modes need a full uninterrupted 30-minute sequence after its UTC boundary. Tuning, stopping or changing modes discards partial audio. Sequence lengths share audio blocks. Additional modes use a separate bounded worker, with at most four capture channels, 96 MiB of capture audio and 32 queued jobs referencing at most 96 MiB of unique audio. Expired or excess jobs are skipped and counted. See [mode configuration and validation](docs/MULTIMODE.md).

Click a station row or use Up/Down in its table to select and highlight it on the map. Double-click a table row or click a map station to open details. Closing details keeps the station selected.

### JS8 reception

Enable **JS8** in **Modes...** or **Settings > Reception**. It starts off and remembers your choice. **Speed: All** receives Normal (15 s), Fast (10 s), Turbo / JS8 40 (6 s) and Slow (30 s); experimental JS8 60 is excluded. Tune a capture covering a JS8 dial preset, such as **7.078 MHz** or **14.078 MHz**. JS8 uses octagonal map markers and the existing tables, filters, home distances and history.

The receive-only decoder and message parser come from pinned [JS8Call](https://github.com/js8call/js8call) source, developed by **Jordan Sherer KN4CRD**, **Allan Bazinet W6BAZ** and contributors, building on WSJT-family work. Longer messages appear as frames arrive and are marked **incomplete** until their final frame and any required checksum pass. Missing headers remain unattributed fragments; ordinary message text is never treated as proof of a sender or location. A separate bounded JS8 worker keeps its decoding and recovery apart from FT8/FT4. [JS8 behavior and limits](docs/MULTIMODE.md#js8-reception).

## Screenshots

Live reception in SDR++Brown, showing the DX map, reception tables, country flags and waterfall notifications.

![Live FT8/FT4 reception with the plugin and waterfall notifications](docs/images/live-reception-overview.jpg)

![Decoder workspace with country statistics, map and unique stations table](docs/images/decoder-workspace.jpg)

## Read the numbers correctly

Distance uses the great-circle path between your home grid center and a received station grid center. `*` before a distance marks recent cached grid evidence. **Approximate unknown distances based on country** is available in Settings > General and starts on. `[C]` marks a near-side country-grid estimate when normal grid evidence is unavailable. It works with existing and new visible receptions and stays separate from station positions and distance records. Country-name variants are recognized, including European/Asiatic Russia and Turkey; these regional labels use the whole-country outline for this coarse estimate. Missing or ambiguous outlines stay unknown. A callsign prefix identifies a DXCC entity rather than an exact transmitter location. See the [distance explanation](docs/USER_GUIDE.md#colors-and-distance-evidence) for limits.

**DX activity bearing** and its compass follow the shared reception filters and the current session/history scope. They summarize short-path bearings from home to the farthest matching stations in the selected time window. All with saved history includes retained historical and live receptions; Last 15 minutes limits the sample to those 15 minutes. Historical/custom date ranges use matching past receptions without an additional age cutoff. All supported modes can use received/cached grids or, when country estimates are enabled, the existing near-side country-grid approximation. `[C]` marks a bearing containing country estimates; hover for the sample and estimate count. Country-only directions are coarse and cannot locate the transmitter within its country. It describes observed reception activity in the selected period, not measured arrival direction or a propagation forecast. An older history selection does not describe current conditions. An addressed message and an arc do not prove a completed contact.

[User guide](docs/USER_GUIDE.md) explains the columns, colors, settings, map cache, scientific limits and GridTracker setup. The same essentials are available in the plugin's **Help** dialog.

## CPU and memory on macOS

Measured on an Apple M2 Max with 12 cores and 32 GB RAM, receiving 40m FT8 and FT4 from a 3.2 MS/s SDR capture. Each state ran for about 60 seconds with the same 1512 x 921 Brown window. Totals include Brown and the decoder helper.

| State | Average CPU | Average memory |
| --- | ---: | ---: |
| Plugin unloaded | 30.5% | 576 MB |
| Decoding, window closed | 34.6% | 901 MB |
| Decoding, map and tables visible | 47.5% | 873 MB |
| Sidebar switch off after decoding | 28.4% | 941 MB |

100% CPU means one core. Memory is macOS physical footprint in decimal MB. These are short measurements on quiet live RF; decoding load, map rendering and caches affect the results. Closing the window reduced CPU while reception continued. Switching the plugin off stopped decoding but retained an idle helper and cached memory; unloading it is a different state. The active helper averaged about 1% CPU and 277 MB. No separate mapper helper was used; GridTracker and the legacy decoder were off.

## Build and contribute

See [building and releasing](docs/RELEASING.md) and the [Windows build guide](docs/WINDOWS.md). The decoder uses pinned SDR++Brown/MSHV code, with FFT plan lifetime fixes. History uses SQLite and polygon triangulation uses Mapbox Earcut. macOS dialogs and sound use AppKit; Windows uses native Windows dialogs and a separate decoder process connected through the SDR# SDK. [Third-party notices](THIRD_PARTY_NOTICES.md).

## Feedback wanted

Also try [Maverick Text Modes Decoder: CW / RTTY / JS8 / Olivia / PSK31 / PSK63](https://github.com/ReLLL/maverick-text-modes-decoder), our companion plugin for text modes. Test both plugins and share reception reports, bugs or ideas through their GitHub Issues or [@FoxOnTheRunTr](https://x.com/FoxOnTheRunTr).

We need your feedback: reception results, bugs, feature ideas and general UI suggestions. Post to **[@FoxOnTheRunTr on X / Twitter](https://x.com/FoxOnTheRunTr)** or open a **[GitHub issue](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/issues)**. Tell us what worked and what felt confusing.

For a bug report, include the plugin version, SDR application/build, operating system, radio hardware, mode and steps to reproduce it. Relevant messages or screenshots help; review them for private information before sharing.
