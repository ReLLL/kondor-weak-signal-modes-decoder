# Third-party notices

## Windows SDR# target

SDR# and its plugin SDK are supplied by [Airspy](https://airspy.com/download/). SDK reference assemblies are used at build time and are not redistributed in the plugin package. The adapter calls SDR#'s installed native DSP APIs. SDR# remains a separate application with its own terms.

Windows workers use MSYS2 UCRT64 runtime packages, including GCC/MinGW-w64, FFTW, SQLite, curl and its dependencies, GLFW and dynamically linked Qt Core. Retain the exact runtime versions, package hashes, original licenses and corresponding-source requirements with any public Windows release. The macOS static Qt provenance below applies to macOS builds; Windows uses its separately recorded MSYS2 Qt package.

The selected Windows graphics under `resources/windows-icons/` come from Twemoji 16.0.1, commit `50c7abfe6813680455781862f7b34305cd1eb9f5`, and use CC BY 4.0. Original attribution and license text are retained in `LICENSE-GRAPHICS`; `source.json` records the source archive and file hashes. Graphics are scaled for display. See [Twemoji](https://github.com/jdecked/twemoji) and the retained license.

See [CREDITS.md](CREDITS.md) for acknowledgements of the protocol designers, decoder developers and other upstream contributors. Original copyright and license notices remain authoritative.

## WSJT development group copyright

The following notice is reproduced from the pinned WSJT-X `doc/common/license.adoc`, with its program-name placeholder expanded to WSJT-X:

> The algorithms, source code, look-and-feel of WSJT-X and related programs, and protocol specifications for the modes FSK441, FST4, FST4W, FT4, FT8, JT4, JT6M, JT9, JT44, JT65, JTMS, Q65, QRA64, ISCAT, and MSK144 are Copyright (C) 2001-2026 by one or more of the following authors: Joseph Taylor, K1JT; Bill Somerville, G4WJS; Steven Franke, K9AN; Nico Palermo, IV3NWV; Uwe Risse, DG2YCB; Brian Moran, N9ADG; John Nelson, G4KLA; Charles Suckling, DL3WDG; Roger Rehr, W3SZ; Greg Beam, KI7MT; Michael Black, W9MDB; Edson Pereira, PY2SDR; Philip Karn, KA9Q; and other members of the WSJT Development Group.

The adapted FT8/FT4 source also retains the copyright notices from its earlier WSJT/MSHV revisions. Full upstream licenses and per-file notices accompany the corresponding source.

## SDR++Brown and MSHV

The plugin integrates with [SDR++Brown](https://github.com/sannysanoff/SDRPlusPlusBrown), pinned at `679f48deee5299a199d15d30e6e785fac6054896`. Its FT8/FT4 decoder contains adapted MSHV code and upstream author notices. GPL terms apply. The staging script applies FFT plan lifetime fixes, without changing decoder sensitivity or algorithms. Release assets include the staged corresponding source and upstream license notices.

## JS8Call and its helper dependencies

The receive-only JS8 helper uses unchanged decoder and Varicode source from [JS8Call](https://github.com/js8call/js8call), commit `a7ff1be0b389d287fdc56e2ea0d06962aa68127d`. Original notices credit Jordan Sherer KN4CRD, Allan Bazinet W6BAZ and other JS8Call/WSJT contributors. GPLv3 terms and per-file notices are retained in `third_party/js8call/`; `source-pin.json` records exact hashes. Upstream audio fixtures are retained for testing and are excluded from installed payloads.

The helper statically links Qt Core 6.8.2 from the verified upstream qtbase source archive (SHA-256 `012043ce6d411e6e8a91fdc4e05e6bedcfa10fcb1347d3c33908f7fdd10dfe05`). GPLv3 terms are used for this GPL project. Qt and its bundled third-party notices ship in `COPYING.QT`; the corresponding-source archive includes the complete matching Qt source archive, source checksum and build recipe. Rebuilding and replacing the helper are permitted under those terms. The main host plugin does not link Qt.

JS8Call's vendored Eigen headers retain MPL-2.0 and individual file notices. CRC++ retains its BSD 3-Clause license. `COPYING.JS8` accompanies the helper, alongside `js8-runtime.json` provenance. Boost headers use the Boost Software License 1.0; the host's FFTW library retains its own notices. No Qt or Homebrew installation is required to run the packaged helper.

## Mapbox Earcut

`third_party/earcut/earcut.hpp` is the unmodified header from [Mapbox Earcut](https://github.com/mapbox/earcut.hpp), version 3.2.4, commit `177bd66b5338ab66f4e9ce8f3cca418b9463d809`. Its ISC license is included in `third_party/earcut/LICENSE`.

## WSJT-X native receive decoders

The additional-mode helpers use unmodified WSPR, FST4/FST4W and Q65 decoder routines from [WSJT-X](https://github.com/WSJTX/wsjtx), commit `967c85a6119110788b0df3782cf796485b2cee14`. The staged subsets and per-file SHA-256 values are recorded in `third_party/wsjtx/source-pin.json` and `mode-source-pin.json`; GPLv3 is included as `third_party/wsjtx/COPYING`. Original author notices are retained. This project adds native framing and receiver adapters. Separate reference tests use upstream encoders and do not install test executables. A Fortran compiler is a build dependency; users need no compiler or Python runtime.

## Map data and providers

The bundled world country geometry is derived from [Natural Earth](https://www.naturalearthdata.com/about/terms-of-use/) public-domain data. Online maps are selectable Esri World Street Map and Humanitarian OpenStreetMap. Visible map credits link to the providers and contributors. Online tile caching is bounded and respects response cache directives; no bulk download is implemented.

See [OpenStreetMap attribution](https://www.openstreetmap.org/copyright), [OpenStreetMap France usage](https://www.openstreetmap.fr/usage/) and [Esri credits](https://doc.arcgis.com/en/arcgis-online/reference/display-copyrights.htm). Provider availability and terms can change.

## Runtime and system components

The host provides SDR++ core. macOS packages bundle private, dynamically linked copies of the build host's VOLK and single-precision FFTW runtimes. Their algorithm implementations are unchanged; private install names isolate them from host runtime upgrades. VOLK is GPL-3.0-or-later and FFTW is GPL-2.0-or-later. Retain `COPYING.DSP` and `dsp-runtime.json` with the libraries; the latter records exact original binary hashes and checksum-pinned upstream source archives. Full corresponding source and its build files accompany the source package under `build-dsp-runtime/source/`. Users may replace/rebuild these dynamic libraries under the applicable licenses. SQLite, curl, AppKit, AudioToolbox and OpenGL use the macOS environment.

Additional-mode builds bundle GCC 14.3 `libgfortran`, `libquadmath` and `libgcc_s` runtime libraries from the verified Homebrew `arm64_sequoia` bottle. Their arithmetic implementations are unchanged; copied Mach-O install names use relative paths and binaries are signed for distribution. GNU GPLv3 with the GCC Runtime Library Exception applies where specified upstream; libquadmath uses GNU LGPL 2.1-or-later. `COPYING.RUNTIME` and `COPYING.LIB` accompany the payload. Runtime provenance records the bottle checksum and Homebrew formula revision. The corresponding-source archive also includes the full GCC 14.3 source archive, the matching Darwin patch and the Homebrew build recipe under `build-mode-runtime/runtime-source/`. Library replacement and rebuilding are permitted under the applicable licenses.

## GridTracker compatibility

The receive-only UDP encoder follows [WSJT-X NetworkMessage schema 2](https://github.com/WSJTX/wsjtx/blob/master/Network/NetworkMessage.hpp). GridTracker and WSJT-X are independent projects; this plugin is not affiliated with them.

## Amateur Radio Country Files

The standard SDR++ build bundles CTY.DAT country-prefix data maintained by Jim Reisert, AD1C. The snapshot matches the data used by the pinned Brown host. Format and source: https://www.country-files.com/cty-dat-format/ . Redistribution permission is reproduced in `resources/cty-LICENSE.txt`. Country identity is inferred from prefixes and exact-call exceptions; these records do not locate an individual transmitter.

## Standard SDR++ host headers

The standard build uses headers from SDR++ commit `762444d3406d68fbc9d4dff8cc237d382793a6e3` under GPL-3.0. The required source and upstream license are included in the corresponding-source archive. It links to the user's separately installed standard SDR++ runtime, not Brown's host libraries.
