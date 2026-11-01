"""Install Kondor Weak Signal Modes Decoder outside the host's signed app, with reversible backups."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from mode_payload import mode_files, NATIVE_BINARIES

ROOT = Path(__file__).resolve().parents[1]
FILES = ("dx_notebook.dylib", "dx_decoder", "world.geojson")
INSTANCE_NAME = "Kondor Weak Signal Modes Decoder"
PREVIOUS_NAMES = ("Kondor Advanced Decoder", "Advanced FT8/FT4/WSPR DX Decoder", "Advanced FT8/FT4 DX Decoder", "DX Notebook")
MODULE_NAME = "advanced_ft8_ft4_dx_decoder"


def host_is_running(host, host_app=None):
    app = host_app or Path("/Applications") / ("SDR++Brown.app" if host == "brown" else "SDR++.app")
    probe = subprocess.run(["pgrep", "-f", re.escape(str(app / "Contents/MacOS/sdrpp"))], capture_output=True)
    if probe.returncode not in (0, 1):
        raise ValueError("Unable to verify the radio application's process state")
    return probe.returncode == 0


def validate_runtime(build, host, host_app=None):
    app = host_app or Path("/Applications") / ("SDR++Brown.app" if host == "brown" else "SDR++.app")
    # Same native checker as the GUI; customers need no Xcode or Homebrew tools.
    import plistlib
    with (app / "Contents/Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    expected = "oorg.sdrpp.sdrppbrown" if host == "brown" else "oorg.sdrpp.sdrpp"
    if info.get("CFBundleIdentifier") != expected:
        raise ValueError("The selected application is not the matching radio host")
    environment = {key: value for key, value in os.environ.items() if not key.startswith("DYLD_")}
    commands = [[build / "dx_host_probe", app / "Contents/MacOS/sdrpp", app / "Contents/Frameworks/libsdrpp_core.dylib", build / "dx_notebook.dylib"]]
    commands += [[build / name, "--check-runtime"] for name in ("dx_decoder", "dx_modes_decoder", "dx_wspr_decoder", "dx_js8_decoder") if (build / name).is_file()]
    for command in commands:
        with tempfile.TemporaryFile() as log:
            try:
                result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=log, stderr=log, env=environment, timeout=30)
            except subprocess.TimeoutExpired as error:
                raise ValueError("Host compatibility check timed out; nothing was changed") from error
            if result.returncode:
                log.seek(0)
                raise ValueError("Host compatibility check failed; nothing was changed. Reinstall the radio app or use a matching plugin build.\n" + log.read(8192).decode(errors="replace"))


def install(build, profile, host="brown", host_app=None, checkpoint=lambda _: None):
    config = profile / "config.json"
    if config.is_symlink():
        raise ValueError("Configuration is a symbolic link; nothing was changed")
    original_config = config.read_bytes()
    data = json.loads(original_config)
    instances = data.setdefault("moduleInstances", {})
    existing = instances.get(INSTANCE_NAME)
    previous = [instances[name] for name in PREVIOUS_NAMES if name in instances]
    legacy = next(iter(previous), None)
    if existing and existing.get("module") not in (MODULE_NAME, "dx_notebook"):
        raise ValueError("Kondor Weak Signal Modes Decoder name belongs to another module")
    if any(value.get("module") not in (MODULE_NAME, "dx_notebook") for value in previous):
        raise ValueError("Legacy DX Notebook name belongs to another module")
    target = profile / "plugins/dx-notebook"
    for path in (target.parent, target, profile / "dx-notebook-backups"):
        if path.is_symlink():
            raise ValueError("Plugin or backup directory is a symbolic link; nothing was changed")
    sources = {"dx_notebook.dylib": build / "dx_notebook.dylib",
               "dx_decoder": build / "dx_decoder", "world.geojson": ROOT / "resources/world.geojson"}
    sources.update({name: build / name for name in mode_files(build)})
    manifest = json.loads((build / "dx_notebook.host.json").read_text())
    if manifest.get("host") != host:
        raise ValueError("The selected radio host requires its matching plugin build")
    if host == "sdrpp":
        sources["cty.dat"] = ROOT / "resources/cty.dat"
    if not all(p.is_file() for p in sources.values()):
        raise ValueError("Build or map resource missing")
    app = host_app or Path("/Applications") / ("SDR++Brown.app" if host == "brown" else "SDR++.app")
    core = app / "Contents/Frameworks/libsdrpp_core.dylib"
    validated_core = hashlib.sha256(core.read_bytes()).digest()
    validate_runtime(build, host, host_app)
    if host_is_running(host, host_app):
        raise ValueError("Quit the selected radio app before installation")
    modules = data.setdefault("modules", [])
    plugin = str(target / "dx_notebook.dylib")
    if plugin not in modules:
        modules.append(plugin)
    instances[INSTANCE_NAME] = dict(existing or legacy or {})
    instances[INSTANCE_NAME].update({"module": MODULE_NAME, "enabled": (existing or legacy or {}).get("enabled", True)})
    for name in PREVIOUS_NAMES:
        instances.pop(name, None)
    menu = data.setdefault("menuElements", [])
    for item in menu:
        if item.get("name") in PREVIOUS_NAMES:
            item["name"] = INSTANCE_NAME
    seen = False
    renamed_menu = []
    for item in menu:
        if item.get("name") == INSTANCE_NAME:
            if seen:
                continue
            seen = True
        renamed_menu.append(item)
    if not seen:
        renamed_menu.insert(0, {"name": INSTANCE_NAME, "open": True})
    data["menuElements"] = renamed_menu
    backup = profile / "dx-notebook-backups" / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d-%H%M%S-%fZ")
    backup.mkdir(parents=True)
    shutil.copy2(config, backup / "config.json")
    if target.exists():
        shutil.copytree(target, backup / "plugin", symlinks=True)
    with tempfile.TemporaryDirectory(prefix=".dx-install-", dir=profile) as temporary:
        stage = Path(temporary)
        plugin = stage / "plugin"
        had_plugin = target.exists()
        if had_plugin:
            shutil.copytree(target, plugin, symlinks=True)
        else:
            plugin.mkdir()
        for name, source in sources.items():
            destination = plugin / name
            if destination.exists() or destination.is_symlink():
                destination.unlink()
            shutil.copy2(source, destination)
            if hashlib.sha256(source.read_bytes()).digest() != hashlib.sha256(destination.read_bytes()).digest():
                raise ValueError("Copied payload failed verification; nothing was replaced")
            if name == "dx_decoder" or name in NATIVE_BINARIES:
                destination.chmod(0o755)
        staged_config = stage / "config.json"
        updated_config = (json.dumps(data, indent=4) + "\n").encode()
        staged_config.write_bytes(updated_config)
        staged_config.chmod(config.stat().st_mode & 0o777)
        checkpoint("staged")
        if hashlib.sha256(core.read_bytes()).digest() != validated_core:
            raise ValueError("The radio app changed during installation; nothing was replaced. Retry with the updated app.")
        validate_runtime(plugin, host, host_app)
        if host_is_running(host, host_app) or config.read_bytes() != original_config:
            raise ValueError("Host started or configuration changed; nothing was replaced")
        target.parent.mkdir(parents=True, exist_ok=True)
        previous = stage / "previous-plugin"
        moved_old = moved_new = committed = False
        try:
            if had_plugin:
                os.replace(target, previous); moved_old = True
            os.replace(plugin, target); moved_new = True
            checkpoint("plugin-replaced")
            if hashlib.sha256(core.read_bytes()).digest() != validated_core:
                raise ValueError("The radio app changed during installation; installation canceled. Retry with the updated app.")
            if host_is_running(host, host_app) or config.read_bytes() != original_config:
                raise ValueError("Host started or configuration changed; installation canceled")
            os.replace(staged_config, config); committed = True
            checkpoint("configuration-replaced")
        except Exception:
            try:
                if committed and config.read_bytes() == updated_config:
                    shutil.copy2(backup / "config.json", stage / "restored-config.json")
                    os.replace(stage / "restored-config.json", config)
                if moved_new:
                    shutil.rmtree(target)
                if moved_old:
                    os.replace(previous, target)
            except Exception as recovery:
                raise RuntimeError("Automatic recovery was incomplete; restore the backup at " + str(backup)) from recovery
            raise
    return backup


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build-native")
    parser.add_argument("--host", choices=("brown", "sdrpp"), default="brown")
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--host-app", type=Path, help="Matching radio app (source builds must use the same host path)")
    args = parser.parse_args()
    profile = args.profile or Path.home() / "Library/Application Support" / ("sdrpp-brown" if args.host == "brown" else "sdrpp")
    if host_is_running(args.host, args.host_app):
        parser.error("Quit the selected radio application before installation; it saves settings on exit")
    backup = install(args.build.resolve(), profile.resolve(), args.host, args.host_app)
    print("Installed candidate. Legacy decoding and bridge preserved.")
    print("Backup:", backup)


if __name__ == "__main__":
    main()
