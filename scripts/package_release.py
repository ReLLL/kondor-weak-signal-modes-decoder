"""Package the plugin and matching source without profile data or build intermediates."""
import argparse
import hashlib
import json
import plistlib
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
import time
from build_installer import build_installer, sign
from mode_payload import mode_files, NATIVE_BINARIES, DSP_FILES
from stage_runtime_sources import prepare as prepare_runtime_sources
from install import validate_runtime

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_FILES = ("README.md", "CREDITS.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md", "CMakeLists.txt", ".gitignore", "install-linux.sh")
SOURCE_DIRS = ("src", "scripts", "tests", "docs", "installer", "cmake", "windows", "linux", "resources/windows-icons", "third_party/earcut", "third_party/wsjtx", "third_party/js8call")


def notarize(path, profile):
    result = subprocess.run(["xcrun", "notarytool", "submit", str(path), "--keychain-profile", profile,
                             "--wait", "--output-format", "json"], check=True, capture_output=True, text=True)
    response = json.loads(result.stdout)
    print("Notarization:", response.get("status"), response.get("id"), flush=True)
    if response.get("status") != "Accepted":
        raise ValueError("Apple did not accept notarization; inspect notarytool log for submission " + str(response.get("id", "unknown")))
    staple(path)


def staple(path):
    # Apple's ticket service can lag behind an Accepted submission.
    for attempt in range(3):
        result = subprocess.run(["xcrun", "stapler", "staple", "-v", str(path)])
        if result.returncode == 0:
            subprocess.run(["xcrun", "stapler", "validate", str(path)], check=True)
            return
        if attempt < 2:
            time.sleep(5)
    raise subprocess.CalledProcessError(result.returncode, result.args)


def anonymous_owner(info):
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.pax_headers = {}
    return info


def verify_disk(path):
    result = subprocess.run(["hdiutil", "verify", str(path)], capture_output=True, text=True)
    if result.returncode and "Resource temporarily unavailable" in result.stderr:
        # macOS can leave this newly created APFS image attached after create.
        # Detach only our exact output image; never touch other mounted disks.
        info = plistlib.loads(subprocess.check_output(["hdiutil", "info", "-plist"]))
        for item in info.get("images", []):
            if item.get("image-path") != str(path.resolve()):
                continue
            devices = [part.get("dev-entry", "") for part in item.get("system-entities", [])]
            device = next((value for value in devices if re.fullmatch(r"/dev/disk[0-9]+", value)), None)
            if device:
                subprocess.run(["hdiutil", "detach", device], check=True)
                result = subprocess.run(["hdiutil", "verify", str(path)], capture_output=True, text=True)
            break
    print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", flush=True)
    result.check_returncode()


def source_files():
    for name in PUBLIC_FILES:
        yield ROOT / name
    yield ROOT / "resources/world.geojson"
    yield ROOT / "resources/share-hf-background.png"
    yield ROOT / "resources/cty.dat"
    yield ROOT / "resources/cty-LICENSE.txt"
    for name in SOURCE_DIRS:
        for path in sorted((ROOT / name).rglob("*")):
            excluded = {"__pycache__", ".git"}
            if name == "windows":
                excluded.update(("bin", "obj"))
            if path.is_file() and not excluded.intersection(path.relative_to(ROOT / name).parts):
                yield path
    runtime_sources=ROOT / "build-mode-runtime/runtime-source"
    if runtime_sources.exists():
        for path in sorted(runtime_sources.iterdir()):
            if path.is_file() and not path.name.endswith(".partial"):
                yield path
    js8_runtime = ROOT / "build-js8-runtime"
    if (js8_runtime / "complete.json").exists():
        yield js8_runtime / "complete.json"
        for path in sorted(js8_runtime.glob("qtbase-*.tar.xz")):
            yield path
    for path in sorted((ROOT / "build-dsp-runtime/source").glob("*.tar.gz")):
        yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build-release")
    parser.add_argument("--signing-identity", default="-", help="Developer ID certificate or '-' for local ad-hoc packages")
    parser.add_argument("--notary-profile", help="Saved notarytool Keychain profile; required for frictionless public installers")
    parser.add_argument("--brown-app", type=Path, default=Path("/Applications/SDR++Brown.app"))
    parser.add_argument("--sdrpp-build", type=Path, help="Matching standard SDR++ build to include in the same installer")
    parser.add_argument("--sdrpp-app", type=Path, default=Path("/Applications/SDR++.app"))
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist", help="Local package destination")
    args = parser.parse_args()
    if args.notary_profile and args.signing_identity == "-":
        parser.error("Notarization requires a Developer ID signing identity")
    version = re.search(r"VERSION (\d+\.\d+\.\d+)", (ROOT / "CMakeLists.txt").read_text()).group(1)
    for build, host in [(args.build, "brown")] + ([(args.sdrpp_build, "sdrpp")] if args.sdrpp_build else []):
        if not all((build / name).is_file() for name in DSP_FILES):
            raise ValueError("Private DSP runtime or compatibility checker is missing; rebuild all targets")
        manifest = json.loads((build / "dx_notebook.host.json").read_text())
        if manifest != {"host": host, "version": version}:
            raise ValueError("Binary host/version manifest does not match this release")
    name = "kondor-weak-signal-modes-decoder-" + version
    if mode_files(args.build):
        prepare_runtime_sources(ROOT / "build-mode-runtime/runtime-source")
    output = args.output_dir
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dx-release-") as temporary:
        package = Path(temporary) / name
        for relative in ("README.md", "CREDITS.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md", "scripts/install.py", "scripts/mode_payload.py", "resources/world.geojson", "resources/cty.dat", "resources/cty-LICENSE.txt", "third_party/earcut/LICENSE", "docs/INSTALL.md", "docs/HOST_COMPATIBILITY.md", "docs/USER_GUIDE.md", "docs/WINDOWS.md", "docs/LINUX.md", "docs/DECODER_RELIABILITY.md", "docs/SHARE_IMAGE.md", "docs/images/live-reception-overview.jpg", "docs/images/decoder-workspace.jpg"):
            target = package / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        for build, folder in [(args.build, "bin")] + ([(args.sdrpp_build, "bin-sdrpp")] if args.sdrpp_build else []):
            extras=mode_files(build)
            for relative in extras:
                if relative not in NATIVE_BINARIES:
                    (package / folder).mkdir(exist_ok=True)
                    shutil.copyfile(build / relative, package / folder / relative)
            for binary in ("dx_notebook.dylib", "dx_decoder", *[value for value in extras if value in NATIVE_BINARIES]):
                target = package / folder / binary
                target.parent.mkdir(exist_ok=True)
                shutil.copyfile(build / binary, target)
                target.chmod(0o755)
                subprocess.run(["strip", "-S", str(target)], check=True)
                libraries = subprocess.check_output(["otool", "-L", str(target)], text=True)
                if "/opt/homebrew/" in libraries or "/Users/" in libraries or "/usr/local/" in libraries:
                    raise ValueError("Nonportable binary dependency")
                strings = subprocess.check_output(["strings", str(target)])
                if b"/Users/" in strings:
                    raise ValueError("Local user path in release binary")
                sign(target, args.signing_identity, helper=binary in ("dx_decoder", "dx_modes_decoder", "dx_wspr_decoder", "dx_js8_decoder", "dx_host_probe"))
            shutil.copyfile(build / "dx_notebook.host.json", package / folder / "dx_notebook.host.json")
            validate_runtime(package / folder, "brown" if folder == "bin" else "sdrpp",
                             args.brown_app if folder == "bin" else args.sdrpp_app)
        app = build_installer(package, version, args.signing_identity, args.brown_app, args.sdrpp_app if args.sdrpp_build else None)
        if args.notary_profile:
            submission = Path(temporary) / "notarization.zip"
            subprocess.run(["ditto", "-c", "-k", "--keepParent", str(app), str(submission)], check=True)
            result = subprocess.run(["xcrun", "notarytool", "submit", str(submission), "--keychain-profile", args.notary_profile,
                                     "--wait", "--output-format", "json"], check=True, capture_output=True, text=True)
            response = json.loads(result.stdout)
            print("Installer notarization:", response.get("status"), response.get("id"), flush=True)
            if response.get("status") != "Accepted":
                raise ValueError("Apple did not accept installer notarization; submission " + str(response.get("id", "unknown")))
            try:
                staple(app)
            except subprocess.CalledProcessError:
                debug = ROOT / "build-installer"
                debug.mkdir(exist_ok=True)
                preserved = Path(tempfile.mkdtemp(prefix="failed-staple-", dir=debug)) / app.name
                shutil.copytree(app, preserved)
                print("Preserved installer for diagnosis:", preserved, flush=True)
                raise
            subprocess.run(["spctl", "--assess", "--type", "execute", str(app)], check=True)
        archive = output / (name + "-macos-arm64.zip")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
            for path in sorted(package.rglob("*")):
                if path.is_file():
                    bundle.write(path, path.relative_to(package.parent))
        disk = output / (name + "-installer-macos-arm64.dmg")
        subprocess.run(["hdiutil", "create", "-ov", "-format", "UDZO", "-volname", "Kondor Weak Signal Modes Decoder",
                        "-srcfolder", str(package), str(disk)], check=True)
        verify_disk(disk)
        if args.notary_profile:
            sign(disk, args.signing_identity)
            notarize(disk, args.notary_profile)
        verify_disk(disk)
    source = output / (name + "-corresponding-source.tar.gz")
    with tarfile.open(source, "w:gz") as bundle:
        for path in source_files():
            bundle.add(path, arcname=Path(name) / path.relative_to(ROOT), filter=anonymous_owner)
        staged = ROOT / "build"
        for relative in ("license", "source-pin.txt", "core", "decoder_modules/ft8_decoder", "decoder_modules/radio"):
            bundle.add(staged / relative, arcname=Path(name) / "build" / relative, filter=anonymous_owner)
        if args.sdrpp_build:
            for relative in ("license", "source-pin.txt", "core"):
                bundle.add(ROOT / "build-sdrpp-source" / relative, arcname=Path(name) / "build-sdrpp-source" / relative, filter=anonymous_owner)
    checksums = output / (name + "-SHA256SUMS.txt")
    checksums.write_text("".join(hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n" for path in (archive, disk, source)))
    print("Release package, corresponding source and SHA-256 checksums:")
    for path in (archive, disk, source, checksums):
        print(path.name)


if __name__ == "__main__":
    main()
