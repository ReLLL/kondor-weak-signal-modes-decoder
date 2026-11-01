# SDR# Windows target

Read when building, installing or validating the Windows SDR# adapter.

The Windows v1.0.56 preview adds an SDR# adapter and a separate native decoder window. It reuses the map, tables, filters, history, country lookup, notifications and decoder engines. The macOS SDR++ and SDR++Brown integrations remain separate.

Target: **SDR# x64 .NET 9**, using the official Airspy SDK. Validation uses **SDR# 1.0.0.1921** in Windows 11 ARM64 under Parallels, through Windows x64 emulation. The actual host decoded a synthetic FT8 IQ recording into the table, receiver distances, compass and waterfall notifications. Reception continued with the decoder window closed; reopening preserved current data. This target does not cover x86 or .NET 8 SDR# builds. The [GitHub release](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/latest) includes a Windows preview ZIP alongside the macOS packages. Official SDK and host archive hashes are recorded in `windows/source-pin.json`.

## Install and use

Extract the complete Windows ZIP. Close SDR#, run `Install.cmd`, then select `SDRSharp.dotnet9.exe`. The installer verifies package hashes and backs up previous plugin binaries outside the plugins scan directory. It preserves reception data and does not modify SDR# binaries.

To install manually, close SDR# and copy the ZIP's top-level `AdvancedDx` folder into `Plugins` beside `SDRSharp.dotnet9.exe`. Keep its structure: `Plugins\AdvancedDx\SDRSharp.AdvancedDx.dll` is the adapter, and `Plugins\AdvancedDx\AdvancedDx\` holds the native worker and all accompanying files. Use the configured directory instead if `SDRSharp.config` overrides `core.pluginsDirectory`. Move an old plugin folder to a backup outside the plugins directory before upgrading. No `Plugins.xml` entry is needed. [Full manual installation notes](INSTALL.md#windows-installation-sdr).

In SDR#'s plugins panel, enable **Kondor Weak Signal Modes Decoder** and use **Open Kondor Decoder**. Closing its window hides it; reception continues while enabled and the radio is running. Reopen brings the current data to the front. A failed worker can be restarted with Open.

FT8, FT4 and WSPR are enabled initially. Settings provide the other supported modes. Only presets inside the captured spectrum can receive. SDR#'s native downconverter, resampler and filters prepare private copies of the IQ data; the adapter never modifies the host buffer. Decoding runs in bounded, lower-priority processes. Stopping/retuning invalidates incomplete captures. Audio gaps invalidate a sequence rather than joining unrelated samples.

Set **Settings > General > Maidenhead grid** to the receiver's location. Use the [K7FRY locator map](https://k7fry.com/grid/), click the receiver's location and copy its first six characters. Distances and bearings use this home grid, as on macOS. Without a grid those values remain unavailable.

Settings, CSV, SQLite history and cached map tiles live under `%LOCALAPPDATA%\Advanced DX Decoder\SDRSharp`. Each data folder allows one worker at a time. Uninstall by closing SDR# and removing `Plugins\AdvancedDx`; the data folder is retained. Previous binaries are under `AdvancedDxBackups` beside SDR#.

## Build

Use the official [Airspy SDR# plugin SDK](https://airspy.com/download/), .NET 9 SDK, Python 3.12 or later and MSYS2 UCRT64 with GCC, GFortran, CMake, Ninja, FFTW, SQLite, curl, GLFW, Qt 6 Core, Eigen and Boost. Use the pinned decoder/ImGui sources staged by this repository's normal host bootstrap. SDK reference assemblies are build-time references only.

From UCRT64, with the repository's staged sources under `build`:

```sh
cmake -S . -B build-windows -G Ninja -DDX_HOST=sdrsharp -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel 2
ctest --test-dir build-windows --output-on-failure
dotnet build windows/SDRSharp.AdvancedDx/SDRSharp.AdvancedDx.csproj -c Release -p:SdrSharpSdk=C:/path/to/sdk/lib
python windows/tests/decoders.py build-windows/windows
```

`scripts/package_windows.py` accepts the native build, adapter DLL, UCRT64 runtime prefix, objdump and a fresh output folder. It follows native DLL imports recursively, copies runtime licenses, maps and icons, and writes a SHA-256 manifest and ZIP. It never bundles SDR# or SDK reference assemblies. Public releases must also include matching runtime source/provenance and complete the release audit.

Create the separate runtime-source asset with `scripts/package_windows_sources.py --package <windows.zip> --binary-cache <verified-msys2-package-cache> --cache build-windows-runtime-source --output <windows-runtime-source.tar>`. It verifies the cached binary package hashes, maps every shipped runtime DLL to its MSYS2 source package, and bundles the matching source archives, build recipes and ownership/checksum manifest. Publish this asset beside the plugin and the project's corresponding-source archive. The source archives are already compressed; the outer TAR avoids another compression pass.

## Validation and data safety

Windows tests cover Unicode history filenames, CSV/SQLite deletion, date formatting, worker reuse, crash recovery, cancellation, deadlines and descendant cleanup. A Winsock loopback test covers GridTracker forwarding, replay requests and recording exclusion. Installer checks cover fresh installation, a custom plugins folder, upgrade backups, unchanged host configuration and corrupt-package rejection. Packaging rejects binaries containing the builder's private home paths; Fortran uses relative source paths because its runtime diagnostics retain paths despite prefix-map flags.

Decoder fixtures exercise FT8, FT4, WSPR, FST4, FST4W, Q65 and all four JS8 speeds. WSPR also passes through the production helper's child-process audio path. `windows/tests/RadioProbe` checks the production SDR# DSP path with the installed host's actual Radio assembly and native libraries; SDK reference stubs cannot execute these tests. Tests at 192 kHz, 256 kHz, 2.048 MHz and 2.4 MHz verify mixing direction, passband gain within 1 dB and opposite-sideband rejection. These are functional checks, not an RF sensitivity measurement.

Keep synthetic IQ/audio, host configuration and reception data in owned temporary folders. Set `ADVANCED_DX_DATA_ROOT` for isolated adapter tests. After stopping writers, delete every generated reception database, CSV and decoder log. Retain aggregate results and fixture hashes only. Never mix sample recordings into normal reception history.

These functional tests do not establish RF sensitivity, false-decode rate, compatibility with every SDR# version or continuous operation over months. Use both macOS host regression suites after shared portability changes. See [credits](../CREDITS.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).
