"""Build the native installer with Apple frameworks only; Python is a build-time tool."""
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import re
from mode_payload import mode_files, NATIVE_BINARIES, DSP_FILES

ROOT = Path(__file__).resolve().parents[1]
APP_NAME = "Install Kondor Weak Signal Modes Decoder.app"


def sign(path, identity, helper=False):
    args = ["codesign", "--force", "--sign", identity]
    if identity != "-":
        args += ["--timestamp", "--options", "runtime"]
        if helper:
            args += ["--entitlements", str(ROOT / "installer/helper-entitlements.plist")]
    subprocess.run(args + [str(path)], check=True)
    subprocess.run(["codesign", "--verify", "--strict", str(path)], check=True)


def add_payload(package, contents, folder, binary_folder, host_app, version):
    if not all((package / binary_folder / name).is_file() for name in DSP_FILES):
        raise ValueError("Build the private DSP runtime and compatibility checker before packaging")
    payload = contents / "Resources" / folder
    payload.mkdir(parents=True)
    sources = {"dx_notebook.dylib": package / binary_folder / "dx_notebook.dylib",
               "dx_decoder": package / binary_folder / "dx_decoder", "world.geojson": package / "resources/world.geojson"}
    sources.update({name: package / binary_folder / name for name in mode_files(package / binary_folder)})
    with (host_app / "Contents/Info.plist").open("rb") as stream:
        host_info = plistlib.load(stream)
    if host_info["CFBundleIdentifier"] == "oorg.sdrpp.sdrpp":
        sources["cty.dat"] = package / "resources/cty.dat"
    for name, source in sources.items():
        shutil.copyfile(source, payload / name)
        (payload / name).chmod(0o755 if name.endswith(".dylib") or name == "dx_decoder" or name in NATIVE_BINARIES else 0o644)
    manifest = {"version": version, "hostBundleID": host_info["CFBundleIdentifier"],
        "hostVersion": host_info["CFBundleVersion"],
        "hostCoreSHA256": hashlib.sha256((host_app / "Contents/Frameworks/libsdrpp_core.dylib").read_bytes()).hexdigest(),
        "hostLibraries": sorted({name for binary in ["dx_notebook.dylib", "dx_decoder", *[value for value in mode_files(package / binary_folder) if value in NATIVE_BINARIES]] for name in re.findall(r"@rpath/([^\s]+\.dylib)", subprocess.check_output(["otool", "-L", str(package / binary_folder / binary)], text=True)) if name not in sources}),
        "files": {name: hashlib.sha256((payload / name).read_bytes()).hexdigest() for name in sources}}
    (payload / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


def build_installer(package, version, identity, brown_app, sdrpp_app=None):
    # Every package must pass the isolated installer rollback/integrity checks.
    with tempfile.TemporaryDirectory(prefix="dx-installer-check-") as temporary:
        test = Path(temporary) / "installer-tests"
        subprocess.run(["xcrun", "swiftc", "-O", "-module-cache-path", str(Path(temporary) / "module-cache"), str(ROOT / "installer/InstallerCore.swift"), str(ROOT / "installer/HostCompatibility.swift"), str(ROOT / "installer/GridLocator.swift"),
                        str(ROOT / "tests/installer_tests.swift"), "-o", str(test)], check=True)
        subprocess.run([str(test)], check=True)
    app = package / APP_NAME
    contents = app / "Contents"
    executable = contents / "MacOS/Installer"
    (contents / "Resources").mkdir(parents=True)
    executable.parent.mkdir()
    subprocess.run([
        "xcrun", "swiftc", "-O", "-target", "arm64-apple-macos15.0", "-module-cache-path", str(ROOT / "build-installer/module-cache"),
        "-file-prefix-map", str(ROOT) + "=.",
        str(ROOT / "installer/InstallerCore.swift"), str(ROOT / "installer/HostCompatibility.swift"), str(ROOT / "installer/main.swift"),
        str(ROOT / "installer/GridLocator.swift"), str(ROOT / "installer/LocationLookup.swift"),
        "-o", str(executable)
    ], check=True)
    add_payload(package, contents, "Payload", "bin", brown_app, version)
    if sdrpp_app:
        add_payload(package, contents, "PayloadSDRPP", "bin-sdrpp", sdrpp_app, version)
    info = {"CFBundleIdentifier": "com.github.ReLLL.advanced-ft8-ft4-dx-decoder.installer",
            "CFBundleName": "Kondor Weak Signal Modes Decoder Installer",
            "CFBundleDisplayName": "Install Kondor Weak Signal Modes Decoder",
            "CFBundleExecutable": "Installer", "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": version, "CFBundleVersion": version,
            "LSMinimumSystemVersion": "15.0", "NSHighResolutionCapable": True,
            "NSHumanReadableCopyright": "ReLLL. GPL-3.0-or-later.",
            "NSLocationUsageDescription": "Suggest a Maidenhead home grid for receiver distances and bearings. Confirm or edit the suggested grid before installing.",
            "NSLocationWhenInUseUsageDescription": "Suggest a Maidenhead home grid for receiver distances and bearings. Confirm or edit the suggested grid before installing."}
    with (contents / "Info.plist").open("wb") as stream:
        plistlib.dump(info, stream)
    for name in ("README.md", "CREDITS.md", "LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copyfile(ROOT / name, contents / "Resources" / name)
    shutil.copyfile(ROOT / "resources/cty-LICENSE.txt", contents / "Resources/cty-LICENSE.txt")
    # Sign nested code before the enclosing bundle. The payload keeps the exact
    # signatures and hashes of the separately packaged plugin and helper.
    sign(app, identity)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    dependencies = subprocess.check_output(["otool", "-L", str(executable)], text=True)
    if "/opt/homebrew/" in dependencies or "/Users/" in dependencies:
        raise ValueError("Nonportable installer dependency")
    if b"/Users/" in subprocess.check_output(["strings", str(executable)]):
        raise ValueError("Local path in installer executable")
    return app
