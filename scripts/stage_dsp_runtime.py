"""Isolate the exact build-time VOLK/FFTW binaries; never alter the user's host app."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
FILES = ("libkondor_volk.dylib", "libkondor_fftw3f.dylib", "COPYING.DSP", "dsp-runtime.json", "dx_host_probe")
PINS = {
    "volk-3.1.2.tar.gz": ("https://codeload.github.com/gnuradio/volk/tar.gz/refs/tags/v3.1.2", "90082bf68d76d00aa6c35f8bff0e93891b4204d817e1a3ba829486b445f715dc"),
    "volk-3.3.0.tar.gz": ("https://codeload.github.com/gnuradio/volk/tar.gz/refs/tags/v3.3.0", "4face7e38011225fad23b4ad1dcd30bc3e03db26aada6a65fb41a1de21272d89"),
    "fftw-3.3.10.tar.gz": ("https://www.fftw.org/fftw-3.3.10.tar.gz", "56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467"),
    "fftw-3.3.11.tar.gz": ("https://www.fftw.org/fftw-3.3.11.tar.gz", "5630c24cdeb33b131612f7eb4b1a9934234754f9f388ff8617458d0be6f239a1"),
}


def output(*args):
    return subprocess.check_output(args, text=True)


def source(archive, cache):
    if archive not in PINS:
        raise ValueError("Unverified DSP source version: " + archive + "; pin and audit its source before packaging")
    url, digest = PINS[archive]
    path = cache / archive
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        pending = path.with_suffix(".partial")
        try:
            with urllib.request.urlopen(url, timeout=60) as response, pending.open("wb") as stream:
                shutil.copyfileobj(response, stream)
            pending.replace(path)
        finally:
            pending.unlink(missing_ok=True)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise ValueError("DSP source checksum mismatch: " + archive)
    with tarfile.open(path) as bundle:
        licenses = [entry for entry in bundle if entry.isfile() and Path(entry.name).name == "COPYING" and len(Path(entry.name).parts) == 2]
        if len(licenses) != 1:
            raise ValueError("Missing DSP license: " + archive)
        license_text = bundle.extractfile(licenses[0]).read().decode()
    return {"archive": archive, "url": url, "sha256": digest}, license_text


def stage(build, host, cache):
    build.mkdir(parents=True, exist_ok=True)
    records, notices = {}, []
    for pattern, name in [("libvolk.*.dylib", FILES[0]), ("libfftw3f.*.dylib", FILES[1])]:
        candidates = {path.resolve() for path in (host / "Contents/Frameworks").glob(pattern)}
        if len(candidates) != 1:
            raise ValueError("Expected one unambiguous host DSP library: " + pattern)
        original = candidates.pop()
        subprocess.run(["lipo", str(original), "-verify_arch", "arm64"], check=True)
        commands = output("otool", "-arch", "arm64", "-l", str(original))
        minimum = re.findall(r"\bminos (\d+)\.(\d+)", commands)
        minimum += re.findall(r"LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+)\.(\d+)", commands)
        if not minimum or any(tuple(map(int, value)) > (15, 0) for value in minimum):
            raise ValueError("DSP runtime needs newer macOS: " + original.name)
        # These verified libraries depend only on system libraries. Do not silently
        # ship an incomplete closure if a future host adds another dependency.
        dependencies = output("otool", "-L", str(original)).splitlines()
        if any(not line.strip().startswith(("/usr/lib/", "/System/Library/")) for line in dependencies[2:]):
            raise ValueError("Unexpected DSP dependency closure: " + original.name)
        if name == FILES[0]:
            version = re.search(r"current version ([\d.]+)", dependencies[1]).group(1)
            archive = "volk-" + version + ".tar.gz"
        else:
            versions = set(re.findall(rb"fftw-(3\.\d+\.\d+)", original.read_bytes()))
            if len(versions) != 1:
                raise ValueError("Cannot establish FFTW source version")
            archive = "fftw-" + versions.pop().decode() + ".tar.gz"
        provenance, license_text = source(archive, cache)
        install_name = output("otool", "-D", str(original)).splitlines()[1].strip()
        records[name] = {"original_install_name": install_name, "original_sha256": hashlib.sha256(original.read_bytes()).hexdigest(), "source": provenance}
        notices.append(archive + "\n\n" + license_text)
        target = build / name
        shutil.copyfile(original, target); target.chmod(0o755)
        subprocess.run(["install_name_tool", "-id", "@loader_path/" + name, str(target)], check=True)
        subprocess.run(["codesign", "--force", "--sign", "-", str(target)], check=True, capture_output=True)
    (build / "COPYING.DSP").write_text("\n\n".join(notices))
    (build / "dsp-runtime.json").write_text(json.dumps({"minimum_macos": "15.0", "libraries": records}, indent=2) + "\n")


def bind(binary):
    manifest = json.loads((binary.parent / "dsp-runtime.json").read_text())
    libraries = output("otool", "-L", str(binary))
    for name, item in manifest["libraries"].items():
        for old in [item["original_install_name"], "@rpath/" + Path(item["original_install_name"]).name]:
            if old in libraries:
                subprocess.run(["install_name_tool", "-change", old, "@loader_path/" + name, str(binary)], check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True, capture_output=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--host-app", type=Path)
    parser.add_argument("--cache", type=Path, default=ROOT / "build-dsp-runtime/source")
    parser.add_argument("--binary", type=Path)
    args = parser.parse_args()
    if args.binary:
        bind(args.binary.resolve())
    else:
        if not args.build or not args.host_app:
            parser.error("--build and --host-app are required when staging")
        stage(args.build.resolve(), args.host_app.resolve(), args.cache.resolve())
