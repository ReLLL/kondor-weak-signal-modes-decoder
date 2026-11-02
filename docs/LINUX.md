<!-- read_when: building, installing, packaging or troubleshooting Kondor on Linux -->
# Linux: SDR++

The Linux source installer builds Kondor for the computer running it, including FT8, FT4, WSPR, FST4, FST4W, JS8 and Q65. Linux SDR++ support is distributed as a native source-installer archive; macOS and Windows packages remain separate. A macOS `.dylib` or Windows `.exe` cannot be installed on Linux.

Validated in isolated Ubuntu 24.04 ARM64 and x86-64 containers on a Mac (x86-64 uses emulation): host/plugin compilation, all seven decoder modes, history/recovery/retention tests, nine installer safety cases, dependency/ImGui preflight, helper startup, PNG/clipboard handling and fresh-profile host startup with virtual desktop audio. No physical Linux RF receiver was used. Fedora/Arch recipes and other distribution releases still need validation; these are native-build paths, not universal prebuilt binaries.

## Easiest source installation

Download the `linux-source-installer.tar.gz` asset from the [latest release](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/releases/latest), extract it and run `bash install-linux.sh --install-deps` inside the extracted folder. The archive includes the project, prepared share-image background and pinned decoder sources. Or clone the repository as below.

Install Git and Python 3 through your distribution's package manager if they are missing. Then:

```sh
git clone https://github.com/ReLLL/kondor-weak-signal-modes-decoder.git
cd kondor-weak-signal-modes-decoder
bash install-linux.sh --install-deps
```

The package-manager step uses `sudo`. Run the installer itself as your normal user. To inspect the dependency command first, run `bash install-linux.sh --dependencies`. Debian/Ubuntu, Fedora and Arch package recipes are included. Package names and library availability vary by release; the initial validation target is Ubuntu 24.04.

With no host arguments, the installer builds a pinned upstream SDR++ alongside Kondor, then creates **SDR++ with Kondor** in your application menu. It includes RTL-SDR, file playback and the upstream network receivers. Other radio hardware may need additional upstream source modules and libraries. Existing SDR++ installations, profiles, plugins and reception files are kept.

This builds native code, rather than downloading a supposedly universal plugin. The same command chooses the running system's CPU architecture through its native compiler. Linux ARM64 and x86-64 require their own builds; an ARM64 result cannot be copied to an x86-64 computer. Other architectures are not claimed as supported until tested. A first build needs time, disk space and Internet access for dependencies and the pinned host/decoder sources. Subsequent builds reuse the cache. Build parallelism considers available memory; reduce it further with `--jobs 2` or `--jobs 1`.

Optional receiver location:

```sh
bash install-linux.sh --grid KM69 --jobs 2
```

Replace `KM69` with your own Maidenhead locator. The installer does not replace a grid already configured for Kondor. You can also set **Settings > General > Maidenhead grid** later. See the beginner locator instructions in the README. Keep the system clock synchronized with NTP; on systemd distributions, check `timedatectl status`. Your distribution's NTP service must report synchronization, not just be enabled.

## Existing SDR++ installation

For a distribution package or your own SDR++ build, supply its matching source headers, core library and executable:

```sh
bash install-linux.sh \
  --host-source /path/to/the-matching/SDRPlusPlus \
  --host-core /usr/lib/libsdrpp_core.so \
  --host-binary /usr/bin/sdrpp \
  --profile "$HOME/.config/sdrpp"
```

These are example paths. Launch SDR++ once, then quit it normally before installation. An open host is refused; the installer does not kill it. Use the same profile you normally pass to `sdrpp -r`. The installer loads the selected host core and staged plugin in an isolated process, checks dependencies, entry points and the ImGui data layout, then checks each decoder helper before changing the profile. Matching source is still required: this check cannot prove compatibility with arbitrary changed C++ classes.

The installer reads configured resource/module paths, or looks beside the selected host's installation prefix. Use `--resources-dir /path/to/share/sdrpp --modules-dir /path/to/lib/sdrpp/plugins` when those paths cannot be found. The generated launcher provides the selected core's library directory explicitly.

Upstream [SDR++ Linux build instructions](https://github.com/AlexandreRouma/SDRPlusPlus#building-on-linux--bsd) describe the host dependencies. Package installations commonly put plugins in `/usr/lib/sdrpp/plugins` or `/usr/local/lib/sdrpp/plugins`, resources in `/usr/share/sdrpp` or `/usr/local/share/sdrpp`, and the user profile in `~/.config/sdrpp`. The actual `modulesDirectory` and `resourcesDirectory` entries in that profile's `config.json` are authoritative. Custom prefixes, AppImages, Flatpak and Snap can use different paths or sandbox rules. The installer does not modify a mounted AppImage or bypass a sandbox; use the native source installation instead.

SDR++Brown's Linux build is a separate compatibility target. The CMake target accepts `DX_HOST=brown` with matching Brown headers/core, but the automatic source installer selects standard SDR++. Do not install a standard-host payload into Brown without building and validating it against that Brown core. The existing macOS Brown integration is unchanged.

## Files and backups

| Item | Default source-installer location |
| --- | --- |
| Dedicated SDR++ host | `${XDG_DATA_HOME:-~/.local/share}/kondor/hosts/<host revision>/` |
| Dedicated host profile | `${XDG_CONFIG_HOME:-~/.config}/sdrpp-kondor/` |
| Kondor module and helpers | `<profile>/plugins/dx-notebook/` |
| SQLite history, CSV, map cache | `<profile>/dx-notebook/` when using the generated launcher |
| Previous plugin and configuration | `<profile>/kondor-backups/<timestamp>/` |
| Build/source cache | `${XDG_CACHE_HOME:-~/.cache}/kondor-linux/` |
| Launcher | `~/.local/bin/kondor-sdrpp` |

The generated launcher selects the profile, receiver-data folder and host library directory explicitly. A manually launched Linux plugin also uses `<profile>/dx-notebook` unless `ADVANCED_DX_DATA_ROOT` overrides it. Keep using the same host profile for the same history. Never point multiple running hosts at the same reception database.

Installation stages a complete payload beside the target, backs up the previous payload/configuration and rolls back a failed replacement. It does not open, rewrite, truncate or migrate reception history as part of installation. Keep your normal data backup as well.

## Manual build and installation

1. Install the dependencies listed by `bash install-linux.sh --dependencies`.
2. Obtain the exact source for your SDR++ executable/core. Build the host following its upstream instructions if needed.
3. Stage the pinned MSHV decoder sources. Keep third-party source checkouts outside this project:

```sh
git clone https://github.com/sannysanoff/SDRPlusPlusBrown.git /path/to/SDRPlusPlusBrown
python3 scripts/stage_sources.py --source /path/to/SDRPlusPlusBrown
```

The staging script reads the pinned revision; it does not change that checkout's branch. The checkout must contain revision `679f48deee5299a199d15d30e6e785fac6054896`.

4. Configure and build against your selected host:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release \
  -DDX_HOST=sdrpp \
  -DDX_HOST_SOURCE=/path/to/SDRPlusPlus \
  -DDX_HOST_CORE=/path/to/libsdrpp_core.so
cmake --build build-linux -j2
ctest --test-dir build-linux --output-on-failure
cmake --install build-linux --prefix "$PWD/build-linux/payload"
build-linux/dx_host_probe /path/to/libsdrpp_core.so build-linux/payload/dx_notebook.so
```

Use `-DDX_ADDITIONAL_MODES=OFF -DDX_JS8=OFF` only when intentionally building FT8/FT4 alone. Do not present that reduced payload as supporting the other modes.

5. Quit SDR++. Copy the **whole** `payload` folder to `<profile>/plugins/dx-notebook`. Keep `dx_decoder`, the optional helpers, `cty.dat`, `world.geojson`, licenses and the module together. Preserve executable permissions on helpers. Back up the existing folder and `config.json` first.
6. In `config.json`, append the absolute path to `dx_notebook.so` to the `modules` array. Preserve all other module entries and settings. Add one `moduleInstances` entry:

```json
"Kondor Weak Signal Modes Decoder": {
  "module": "advanced_ft8_ft4_dx_decoder",
  "enabled": true,
  "receiverGrid": "YOUR_GRID"
}
```

Replace `YOUR_GRID` with your locator. This is a fragment inside the existing `moduleInstances` object, not a replacement configuration file. Preserve the surrounding JSON structure and commas.

7. Start the selected host with the same profile. The decoder window starts hidden. Click **Open Kondor Decoder** in its sidebar. Check **Settings > General** for the receiver grid, and start the radio on a supported band.

## Troubleshooting

- **Host ABI mismatch / missing symbols:** rebuild with the headers and core for the executable you actually launch. Updating SDR++ may require rebuilding Kondor.
- **Library not found:** use the generated launcher, or include the selected core's directory in your normal dynamic-library search path. Check `ldd dx_notebook.so` and each helper. Install missing packages through the distribution package manager; do not copy unrelated libraries from another system.
- **No receiver:** Kondor does not change USB permissions, detach kernel drivers or install radio drivers. Test your radio in SDR++ first and follow the distribution/host instructions for RTL-SDR permissions. A fresh source installation also registers the installed upstream radio/source modules; upgrading an existing profile preserves its instance choices.
- **Headless/container startup:** compiling and decoder tests can run without a radio, but the SDR++ desktop host needs a display and working audio service. The pinned host's Audio Sink can fail when no audio device/server exists. Linux validation used Xvfb and a PulseAudio null sink; a normal desktop should use its existing PulseAudio/PipeWire/ALSA setup.
- **No decodes:** check NTP synchronization, enabled modes, RF capture coverage, signal levels and complete uninterrupted sequences. WSPR and long FST4/FST4W sequences need more time than FT8.
- **No file dialog/clipboard:** a graphical session with GTK 3 is required for native export controls. Linux alerts currently use the desktop bell; macOS/Windows sound choices retain their existing behavior.
- **Small-memory device:** lower build parallelism. Do not disable confidence protections to reduce resource use.

No Linux SDR# target is provided. SDR# support remains the Windows adapter.
