"""Real Mach-O loader and repair checks; no normal profiles or receptions are used."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from install import install, validate_runtime
from mode_payload import mode_files


def fingerprint(folder):
    return {str(path.relative_to(folder)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in folder.rglob("*") if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--host", type=Path, action="append", required=True)
    parser.add_argument("--kind", choices=("brown", "sdrpp"), required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    # The installer must ignore inherited search-path injection as well.
    os.environ["DYLD_LIBRARY_PATH"] = "/nonexistent-kondor-test-path"
    for app in args.host:
        before = fingerprint(app)
        validate_runtime(build, args.kind, app.resolve())
        assert fingerprint(app) == before, "Host bundle was modified"
        print("PASS real host loader and all helpers:", app.name, flush=True)
    for binary in ["dx_notebook.dylib", "dx_decoder", "dx_modes_decoder", "dx_wspr_decoder", "dx_js8_decoder"]:
        path = build / binary
        if not path.exists():
            continue
        dependencies = subprocess.check_output(["otool", "-L", path], text=True)
        assert "@rpath/libvolk" not in dependencies and "@rpath/libfftw" not in dependencies
        assert "/opt/homebrew/" not in dependencies and "/usr/local/" not in dependencies
        assert "@loader_path/libkondor_" in dependencies
    with tempfile.TemporaryDirectory(prefix="dx-host-compatibility-") as temporary:
        root = Path(temporary)
        host = root / "Host.app"
        shutil.copytree(args.host[-1], host)
        payload = root / "payload"; payload.mkdir()
        for name in ("dx_notebook.dylib", "dx_notebook.host.json", "dx_decoder", *mode_files(build)):
            shutil.copy2(build / name, payload / name)

        profile = root / "profile"; profile.mkdir()
        config = profile / "config.json"
        config.write_text(json.dumps({"modules": [], "moduleInstances": {"Kondor Weak Signal Modes Decoder": {
            "module": "advanced_ft8_ft4_dx_decoder", "enabled": False, "receiverCall": "K1ABC"}}}))
        history = profile / "preserved-history"; history.write_text("fixture history, no reception records")
        # This copied host is never launched. Mock only the process-list query,
        # which the execution sandbox does not expose; all loader checks are real.
        with patch("install.host_is_running", return_value=False):
            install(payload, profile, args.kind, host)
            target = profile / "plugins/dx-notebook"
            before_config, before_plugin = config.read_bytes(), fingerprint(target)
            for point in ["staged", "plugin-replaced", "configuration-replaced"]:
                def fail(checkpoint):
                    if checkpoint == point:
                        raise RuntimeError("injected transaction failure")
                try:
                    install(payload, profile, args.kind, host, checkpoint=fail)
                except RuntimeError as error:
                    assert "injected" in str(error)
                else:
                    raise AssertionError("Accepted failed transaction")
                assert config.read_bytes() == before_config and fingerprint(target) == before_plugin
                assert history.read_text() == "fixture history, no reception records"
                print("PASS Python installer rollback:", point, flush=True)
            (target / "libkondor_volk.dylib").write_bytes(b"damaged installed runtime")
            install(payload, profile, args.kind, host)
            assert fingerprint(target) == before_plugin and config.read_bytes() == before_config
            print("PASS Python installer repairs damaged runtime and preserves settings/history", flush=True)

            core = host / "Contents/Frameworks/libsdrpp_core.dylib"
            original_core = core.read_bytes()
            for point in ["plugin-replaced", "staged"]:
                def update_host(checkpoint):
                    if checkpoint == point:
                        core.write_bytes(original_core + b"\nfixture host update\n")
                try:
                    try:
                        install(payload, profile, args.kind, host, checkpoint=update_host)
                    except ValueError as error:
                        assert "radio app changed" in str(error), error
                    else:
                        raise AssertionError("Accepted host update during " + point)
                    assert config.read_bytes() == before_config and fingerprint(target) == before_plugin
                    assert history.read_text() == "fixture history, no reception records"
                finally:
                    core.write_bytes(original_core)
                print("PASS Python installer rejects host update and rolls back:", point, flush=True)

        def rejected(label, text):
            try:
                validate_runtime(payload, args.kind, host)
            except (ValueError, OSError) as error:
                assert text in str(error), (label, error)
            else:
                raise AssertionError("Accepted " + label)
            print("PASS refusal:", label, flush=True)

        # A missing private runtime must not borrow a similarly named host library.
        library = payload / "libkondor_volk.dylib"
        original = library.read_bytes(); library.unlink()
        rejected("missing private VOLK", "libkondor_volk.dylib")
        library.write_bytes(original); library.chmod(0o755)
        validate_runtime(payload, args.kind, host)
        print("PASS automatic recovery after replacing complete payload", flush=True)

        executable = host / "Contents/MacOS/sdrpp"
        original = executable.read_bytes()
        executable.write_bytes(struct.pack("<II", 0xfeedfacf, 0x01000007))
        rejected("Intel-only host", "Apple Silicon")
        executable.write_bytes(original)

        # Missing host-owned dependencies cannot be repaired with plugin libraries.
        volk = next((host / "Contents/Frameworks").glob("libvolk.*.dylib"))
        original = volk.read_bytes(); volk.unlink()
        rejected("incomplete host runtime", "Host runtime")
        volk.write_bytes(original); volk.chmod(0o755)
        validate_runtime(payload, args.kind, host)

        # A loadable dylib with missing host symbols is still incompatible.
        source = root / "bad.c"
        source.write_text("extern int absent_host_symbol; int* ref = &absent_host_symbol;\n")
        subprocess.run(["clang", "-dynamiclib", "-undefined", "dynamic_lookup", source,
                        "-o", payload / "dx_notebook.dylib"], check=True)
        rejected("unresolved ABI symbol", "absent_host_symbol")
        source.write_text("int _INFO_; void _INIT_(){} void _CREATE_INSTANCE_(){} void _DELETE_INSTANCE_(){} void _END_(){} int _DX_CHECK_HOST_(){return 0;}\n")
        subprocess.run(["clang", "-dynamiclib", source, "-o", payload / "dx_notebook.dylib"], check=True)
        rejected("loadable but incompatible ImGui ABI", "Host ImGui ABI")
    print("Host compatibility checks passed; temporary hosts and payloads removed.")


if __name__ == "__main__":
    main()
