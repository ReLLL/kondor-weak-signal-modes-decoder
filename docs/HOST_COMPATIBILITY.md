# macOS host compatibility

Read when: diagnosing installation, changing host integration, or auditing a package.

## Failure and recovery

Kondor 1.0.53's standard SDR++ plugin recorded `@rpath/libvolk.3.1.dylib`. The real SDR++ 1.3.2 Apple Silicon nightly bundles `libvolk.3.3.dylib`, so both the exact-filename preflight and the dynamic loader rejected it. Reports: [issue 4](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/issues/4), [issue 1](https://github.com/ReLLL/kondor-weak-signal-modes-decoder/issues/1#issuecomment-6314534010).

CMake now stages the actual matching DSP libraries from the build host as `libkondor_volk.dylib` and `libkondor_fftw3f.dylib`. Plugin/helper load commands use `@loader_path` to these files. Each runtime must contain arm64, support macOS 15, have only system dependencies, and have a checksum-pinned matching upstream source archive. The entire group, including licenses, provenance and native checker, is required when packaging. Installer hashes cover every file. Reinstalling replaces the complete plugin payload and preserves preferences/history through the existing backup/rollback transaction.

The selected host still owns `libsdrpp_core.dylib` and its dependency closure. The native `dx_host_probe` loads that exact core first, then the plugin with eager symbol binding, verifies module entry points and checks ImGui version/data layout. It creates no module instance or receiver. Each decoder helper also supports `--check-runtime`, which exits after dependency loading and before decoder initialization, so loader failures are detected before profile writes. The probe exits without running host-global waterfall destructors, which require a GL context. Each check has a thirty-second limit to allow cold code verification under load, captures bounded diagnostics, removes inherited DYLD overrides and runs outside the installer's UI thread. Both GUI and Python installation paths use the native checker; customers need no developer tools.

An incompatible host cannot safely be repaired by substituting a filename. Loader diagnostics direct users to reinstall an incomplete host or obtain a matching plugin build. Successful loading and the ImGui layout check cover the runtime boundaries they exercise; arbitrary future C++ layout or behavioral changes still need matching-header review and real host testing.

## Local validation

```sh
cmake -S . -B build-compat-brown -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
cmake --build build-compat-brown -j4
ctest --test-dir build-compat-brown --output-on-failure
DX_TEST_BUILD=build-compat-brown python3 tests/test_helper.py

cmake -S . -B build-compat-sdrpp -DDX_HOST=sdrpp -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
cmake --build build-compat-sdrpp -j4
ctest --test-dir build-compat-sdrpp --output-on-failure
DX_TEST_BUILD=build-compat-sdrpp python3 tests/test_helper.py

python3 tests/test_host_compatibility.py --kind sdrpp --build build-compat-sdrpp \
  --host /Applications/SDR++.app --host /path/to/isolated/SDR++.app
```

The real-loader regression test checks private runtimes, missing runtime recovery, an Intel-only executable, an incomplete host, unresolved symbols and a loadable but incompatible ImGui ABI. It copies hosts/payloads to owned temporary paths, verifies original host file hashes and deletes its copies. Transaction tests separately cover early failure, timeouts, checksums, settings/history preservation and rollback. Decoder tests use isolated generated input and remove it afterward.

No normal receiver profile or reception history is used for these checks. Public-release gates still require signing/notarization and the existing UI, capture, decoding and recovery checks for supported hosts. macOS 15 remains the binary floor; testing on a newer Mac alone does not establish complete macOS 15 runtime compatibility.

## Recorded verification: 2026-11-01

Local 1.0.55 QA candidate, macOS 26.6.2 on Apple Silicon. Packages use ad hoc signatures; they are not published or notarized.

| Host | Real loader, ImGui check and all helper runtimes | Enabled plugin, host render loop and isolated history writer |
| --- | --- | --- |
| SDR++ 1.2.0, May 2024 | Passed | Passed; zero receptions |
| SDR++ 1.3.2 nightly, asset dated 2026-10-23 | Passed | Passed; zero receptions |
| SDR++Brown 1.2.1, August 2026 | Passed | Passed; zero receptions |

Startup checks used minimal supported host configurations, no hardware/source modules and a separate temporary `ADVANCED_DX_DATA_ROOT` for each process. All test writers stopped before their temporary profiles and databases were removed. Copying a normal profile had stalled before plugin loading; fresh minimal profiles resolved that harness problem. The exact conflicting configuration key was not isolated.

The packaged installer launches outside the execution sandbox. Brown preflight, controlled invalid-profile diagnostics, Retry back to Ready, and Cancel were observed. Copy diagnostics was present and clicked; clipboard contents were not independently verified. Oz separately reported that installation worked; this does not establish a particular host/version or live decoding result.

Real-loader fixtures cover missing/damaged private runtimes, repair with settings/history retained, Intel-only hosts, incomplete host libraries, unresolved symbols and an incompatible ImGui check. Python transactions reject host-core changes during staging/replacement and roll back plugin/configuration failures. Native installer safety tests also pass.

Both decoder suites passed across full runs and targeted reruns, rather than one clean all-tests invocation. Sandbox restrictions required authorized GridTracker/JS8 reruns; a JS8 recovery timeout under concurrent load passed in isolation. MinGW syntax checks cover the changed shared FT8/FT4 and mode-helper entry points; Windows runtime and Qt/JS8 builds remain unverified.

Artifact audit verifies checksums, ZIP executable permissions, strict ad hoc signatures, DMG integrity, DSP corresponding sources and exclusion of local agent instructions. All 45 packaged Mach-O files declare a minimum OS no newer than 15.0 and have portable dependency paths.

Still required before release: real capture/reception/UI checks on the newer host, macOS 15 runtime testing, complete Windows validation when shipping Windows changes, and Developer ID signing/notarization. Loader checks and matching-header review do not prove arbitrary future host ABI compatibility.
