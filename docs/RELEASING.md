# Build and release

Read when: changing host integration, building a matching binary or publishing a release.

## Build requirements

Linux has a separate native CMake target and source installer; see [LINUX.md](LINUX.md). Build against the selected host's exact source/core. The default installer instead builds a pinned, matching SDR++ in the user account. Before advertising a Linux CPU/distribution combination, run the full Linux CTest suite, installer transaction cases, staged helper runtime checks, ELF/ImGui host probe and a fresh-profile desktop-host smoke. The source archive must contain `install-linux.sh`, `linux/` and `docs/LINUX.md`, and must exclude local profiles, reception data, private evidence and AGENTS.md. Linux dependencies come from the distribution; macOS `.dylib` payloads and Windows `.exe` payloads are not Linux assets. Current development evidence covers Ubuntu 24.04 ARM64 and emulated x86-64 containers with virtual audio/display and synthetic decoder fixtures; it does not establish physical RF reception or Fedora/Arch support. Xvfb enables an additional isolated clipboard test without touching the user's desktop clipboard.

For the additional Windows SDR# target, follow [WINDOWS.md](WINDOWS.md). Its native dependencies, SDK and runtime provenance are separate from macOS. Before publishing a Windows asset, verify the installed .NET 9 host, live/replay capture, closing/reopening, worker recovery, installer rollback, complete runtime dependency closure, retained licenses and corresponding source for every shipped library. Do not redistribute SDR# SDK reference assemblies. A development ZIP alone is not a completed public release audit.

For a Windows UI/branding-only rebuild, `DX_PREBUILT_OPTIONAL_HELPERS` may point to verified optional helper executables from a previous release, with `DX_ADDITIONAL_MODES=ON` and `DX_JS8=ON`. Compare all decoder, adapter and vendored sources with that release first, preserve byte-identical helpers and their runtime/source provenance, and record the reused hashes. The CMake path copies the required helpers and rejects missing files; normal builds compile them. Never turn capabilities off to avoid the Fortran/Qt build requirements in a package that advertises these modes.

Apple Silicon macOS, Xcode command-line tools, CMake, Python 3 and matching SDR++ and SDR++Brown applications. Build dependencies can be installed with `brew install cmake ninja fftw volk gcc boost`. Additional modes use the staged, pinned WSJT-X source and a Fortran compiler. Each build uses its selected host's FFTW library and verified macOS 15-compatible GCC runtime copies; do not substitute the developer machine's newer runtime. Runtime provenance and license files ship with both helpers. See [additional modes](MULTIMODE.md).

The pinned host source is `679f48deee5299a199d15d30e6e785fac6054896` in [SDRPlusPlusBrown](https://github.com/sannysanoff/SDRPlusPlusBrown). The staging script copies the required source and applies FFT plan lifetime corrections; it preserves the upstream checkout.

JS8 builds need the pinned static Qt Core runtime once per build machine:

```sh
python3 scripts/build_js8_runtime.py
python3 scripts/stage_js8.py --verify
```

The Qt bootstrap verifies its download checksum and caches a macOS 15-compatible static build. Both host builds share that cache. Normal builds enable DX_JS8; set it OFF to omit the helper. JS8 is still off in receiver preferences by default. Its four-file installer group is independently validated; never ship a helper without its notices and provenance. Packaging includes the full matching Qt source archive and build recipe. Release checks must cover all four JS8 speeds, multi-frame identity/checksums, missing/ambiguous frames, silence/noise, upstream recordings, restart/cancellation and concurrent FT8. Synthetic test data stays in owned temporary folders.

```sh
git clone https://github.com/sannysanoff/SDRPlusPlusBrown.git /tmp/SDRPlusPlusBrown
python3 scripts/stage_sources.py --source /tmp/SDRPlusPlusBrown
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
cmake --build build-release -j4
ctest --test-dir build-release --output-on-failure
DX_TEST_BUILD=build-release python3 tests/test_helper.py
```

For standard SDR++, stage the matching source without changing the upstream checkout:

```sh
git clone https://github.com/AlexandreRouma/SDRPlusPlus.git /tmp/SDRPlusPlus
mkdir -p build-sdrpp-source
git -C /tmp/SDRPlusPlus archive 762444d3406d68fbc9d4dff8cc237d382793a6e3 core license | tar -x -C build-sdrpp-source
echo 762444d3406d68fbc9d4dff8cc237d382793a6e3 > build-sdrpp-source/source-pin.txt
cmake -S . -B build-sdrpp -DDX_HOST=sdrpp -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
cmake --build build-sdrpp -j4
ctest --test-dir build-sdrpp --output-on-failure
DX_TEST_BUILD=build-sdrpp python3 tests/test_helper.py
```

The baseline standard host is SDR++ 1.2.0, May 28, 2024 build. Brown is 1.2.1, August 4, 2026 build. Standard integration uses standard APIs and ImGui hooks, independent receiver settings and bundled CTY.DAT; Brown's native integration stays intact. Both use the pinned MSHV decoder source. CMake stages exact matching VOLK and FFTW binaries under private Kondor install names and binds the plugin/helpers through `@loader_path`. Source archives are checksum-pinned in `scripts/stage_dsp_runtime.py`, cached under `build-dsp-runtime/source/`, and included with corresponding source; `COPYING.DSP` and `dsp-runtime.json` travel with each payload. New upstream runtime versions require an audited source pin. The host bundle remains unchanged. See [host compatibility](HOST_COMPATIBILITY.md).

## Release gate

1. Increment the CMake patch version for the next build. Update README, guide and changelog when behavior changes.
2. Run the native tests above and a separate `-DSANITIZE=ON` build/test. Verify synthetic FT8/FT4 decoding and bounded helper recovery.
3. Check rendered behavior and live RF using isolated profiles for both hosts. Preserve the normal profiles. Standard SDR++ config must use supported host keys: receiverGrid/receiverCall belong inside the plugin's module instance. Do not inject unsupported top-level centerFrequency/viewBandwidth keys; the older host can crash during config repair before any plugin loads.
4. Sanitize generated documentation. Audit source and assets for personal names, emails, credentials, profile data and local absolute paths. Credit the creator as ReLLL; use the GitHub noreply identity for commits.
5. Package with `python3 scripts/package_release.py --build build-release --sdrpp-build build-sdrpp`. Every package automatically compiles/runs the installer safety checks below. The local package builds the combined native installer, signs staged code ad hoc, verifies dependencies, and emits a DMG, ZIP, corresponding source and checksums. Host/version sidecars prevent mixing the two builds. Python and Xcode are build-time tools; customers need neither.
6. For a public installer, add `--signing-identity "$DX_SIGNING_IDENTITY" --notary-profile "$DX_NOTARY_PROFILE"`. Use a Developer ID Application certificate and a saved notarytool Keychain profile. Never put credentials in source. Signing uses hardened runtime; the helper disables library validation because it loads the host's independently signed libraries. The app and DMG must receive Apple's Accepted status, have stapled tickets, and pass Gatekeeper assessment. Failures stop packaging. Certificate-owner details can appear in macOS security information; visible app/repository credits remain ReLLL.
7. Review binary dependencies and deployment targets. Commit named source paths, push, tag `vX.Y.Z` and create the GitHub release with generated assets. Verify public visibility, creator identity and downloaded checksums.

A binary plugin is tied to the host ABI. Rebuilding for a different host requires compatible source/headers and corresponding tests. A lower Mach-O deployment target alone does not establish older-OS runtime compatibility; macOS 15 is the build minimum and macOS 26 is the tested environment for this release.

On macOS, creating an APFS DMG can leave that output image attached and make verification report Resource temporarily unavailable. Verify and detach the exact newly created image before signing/notarization, then verify it again afterward. Packaging retries verification once after detaching its own image. It never detaches unrelated volumes or ignores checksum failures.

## Installer checks

```sh
mkdir -p build-installer
xcrun swiftc -O -target arm64-apple-macos15.0 -module-cache-path build-installer/module-cache installer/InstallerCore.swift installer/HostCompatibility.swift installer/GridLocator.swift tests/installer_tests.swift -o build-installer/installer-tests
build-installer/installer-tests
```

Checks use isolated temporary profiles and synthetic hosts: fresh installation, repeated upgrades, settings/log preservation, executable permissions, backups, rollback at staged/plugin/configuration replacement, running host, corrupt payloads, incompatible host, malformed configuration, symlink refusal, concurrent settings changes, standard payload/country data and receiver setup. Invalid locators must fail before quitting a running host. Keep normal profiles untouched during installer tests.

Inspect the packaged installer window, host selection and Cancel behavior without installing into a normal profile. Inspect both payload manifests/signatures, check the DMG, and verify downloaded release checksums. Host fingerprints and versions are build provenance only. Installation checks host identity, architecture, real dependency and symbol loading, ImGui layout, and helper startup. Test acceptance of rebuilt cores and different version numbers for both hosts. Custom profiles are supported; binaries expect `/Applications/SDR++Brown.app` or `/Applications/SDR++.app` respectively. First-run preparation opens the selected host, waits for its default config, then asks it to quit normally. Existing disabled/enabled preferences are preserved on upgrades; the initial registration is enabled.

Location suggestions use the installer's own macOS Location Services permission and purpose strings. Automatically attempt only for an empty grid; never overwrite a prefilled or manually edited grid. Reject stale or very coarse estimates, show accuracy, and offer manual/map entry. Raw coordinates are not saved in the profile; only the confirmed grid is installed. Location denial must not block installation.
