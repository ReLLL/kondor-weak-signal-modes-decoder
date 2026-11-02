"""Build and install a native Kondor payload without replacing an existing SDR host."""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HOST_PIN = "762444d3406d68fbc9d4dff8cc237d382793a6e3"
DECODER_PIN = "679f48deee5299a199d15d30e6e785fac6054896"
VERSION = re.search(r"project\([^\n]+ VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
TITLE = "Kondor Weak Signal Modes Decoder"


def run(*args, **kwargs):
    print("+", shlex.join(str(x) for x in args), flush=True)
    return subprocess.run([str(x) for x in args], check=True, **kwargs)


def dependencies():
    release = Path("/etc/os-release").read_text() if Path("/etc/os-release").exists() else ""
    if re.search(r"(?:ID|ID_LIKE)=.*(?:ubuntu|debian)", release):
        volk = "libvolk-dev"
        if shutil.which("apt-cache"):
            available = subprocess.run(["apt-cache", "show", volk], capture_output=True)
            if available.returncode or not available.stdout:
                volk = "libvolk2-dev"
        return ["apt-get", "install", "-y", "build-essential", "cmake", "ninja-build", "git", "ca-certificates", "pkg-config", "python3", "gfortran", "libsqlite3-dev", "libcurl4-openssl-dev", "libfftw3-dev", volk, "libglfw3-dev", "libglew-dev", "libzstd-dev", "librtaudio-dev", "librtlsdr-dev", "libgtk-3-dev", "qt6-base-dev", "libboost-dev", "libeigen3-dev", "fonts-dejavu-core", "fonts-noto-color-emoji"]
    if "fedora" in release or "rhel" in release:
        return ["dnf", "install", "-y", "gcc-c++", "gcc-gfortran", "cmake", "ninja-build", "git", "ca-certificates", "pkgconf-pkg-config", "python3", "sqlite-devel", "libcurl-devel", "fftw-devel", "volk-devel", "glfw-devel", "glew-devel", "libzstd-devel", "rtaudio-devel", "rtl-sdr-devel", "gtk3-devel", "qt6-qtbase-devel", "boost-devel", "eigen3-devel", "dejavu-sans-fonts", "google-noto-color-emoji-fonts"]
    if "arch" in release:
        return ["pacman", "-S", "--needed", "base-devel", "cmake", "ninja", "git", "ca-certificates", "pkgconf", "python", "gcc-fortran", "sqlite", "curl", "fftw", "volk", "glfw", "glew", "zstd", "rtaudio", "rtl-sdr", "gtk3", "qt6-base", "boost", "eigen", "ttf-dejavu", "noto-fonts-emoji"]
    return []


def repository(url, pin, target):
    if target.exists():
        head = subprocess.check_output(["git", "-C", str(target), "rev-parse", "HEAD"], text=True).strip()
        if head != pin or subprocess.check_output(["git", "-C", str(target), "status", "--porcelain"]):
            raise ValueError(f"Source cache changed: {target}. Move it aside to rebuild safely.")
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    run("git", "init", target)
    run("git", "-C", target, "remote", "add", "origin", url)
    run("git", "-C", target, "fetch", "--depth=1", "origin", pin)
    run("git", "-C", target, "checkout", "--detach", "FETCH_HEAD")
    return target


def assert_host_stopped(binary, profile):
    """Inspect /proc rather than matching arbitrary command substrings or killing hosts."""
    selected = binary.resolve()
    arguments = {str(binary).encode(), str(selected).encode(), str(profile).encode()}
    for proc in Path("/proc").iterdir():
        if not proc.name.isdigit() or int(proc.name) == os.getpid():
            continue
        try:
            cmd = (proc / "cmdline").read_bytes().split(b"\0")
        except (PermissionError, FileNotFoundError, ProcessLookupError):
            cmd = []
        # An emulated executable or one replaced by an OS update can have an
        # unresolvable /proc/exe link. Its exact argv/profile still identifies it.
        if arguments.intersection(cmd):
            raise ValueError("Close the selected SDR host normally, then rerun. No running host was stopped.")
        try:
            exe = (proc / "exe").resolve(strict=True)
        except (PermissionError, FileNotFoundError, ProcessLookupError):
            exe = None
        if exe == selected:
            raise ValueError("Close the selected SDR host normally, then rerun. No running host was stopped.")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(stream.read()).hexdigest()


def default_jobs():
    jobs = min(4, os.cpu_count() or 2)
    try:
        memory = re.search(r"^MemAvailable:\s+(\d+)", Path("/proc/meminfo").read_text(), re.M)
        if memory:
            jobs = min(jobs, max(1, int(memory[1]) // (1024 * 1024)))
    except OSError:
        jobs = min(jobs, 2)
    return jobs


def atomic_json(path, value):
    fd, name = tempfile.mkstemp(prefix=".kondor-config-", dir=path.parent)
    try:
        with os.fdopen(fd, "w") as out:
            json.dump(value, out, indent=2)
            out.write("\n")
            out.flush()
            os.fsync(out.fileno())
        os.replace(name, path)
    finally:
        if Path(name).exists():
            Path(name).unlink()


def host_defaults(source, modules):
    """Read literal upstream instance defaults; never execute or modify host source."""
    code = (source / "core/src/core.cpp").read_text()
    pattern = r'^\s*defConfig\["moduleInstances"\]\["([^"]+)"\](?:\["module"\])?\s*=\s*"([^"]+)";'
    instances = {name: {"module": module, "enabled": True} for name, module in re.findall(pattern, code, re.M) if (modules / (module + ".so")).is_file()}
    if not instances or "Radio" not in instances:
        raise ValueError("Could not read this host's default receiver modules. Start the host once to create its profile, quit it, then retry with --profile.")
    return instances


def install_payload(build, profile, binary, core, resources, modules, grid="", initial_instances=None):
    profile = profile.expanduser().resolve()
    if profile in (Path.home(), Path("/")):
        raise ValueError("Choose a dedicated SDR host profile, not a home or filesystem root.")
    profile.mkdir(parents=True, exist_ok=True)
    with (profile / ".kondor-install.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return install_payload_locked(build, profile, binary, core, resources, modules, grid, initial_instances)


def install_payload_locked(build, profile, binary, core, resources, modules, grid="", initial_instances=None):
    profile = profile.expanduser().resolve()
    if profile == Path.home() or profile == Path("/"):
        raise ValueError("Choose a dedicated SDR host profile, not a home or filesystem root.")
    profile.mkdir(parents=True, exist_ok=True)
    config_path = profile / "config.json"
    if config_path.is_symlink():
        raise ValueError("A symlinked configuration must be managed manually.")
    original = config_path.read_bytes() if config_path.exists() else None
    config = json.loads(original) if original is not None else {"modulesDirectory": str(modules), "resourcesDirectory": str(resources)}
    if not isinstance(config, dict) or not isinstance(config.get("modules", []), list) or not isinstance(config.get("moduleInstances", {}), dict):
        raise ValueError("Unexpected host configuration; installation has not changed it.")
    config.setdefault("modulesDirectory", str(modules))
    config.setdefault("resourcesDirectory", str(resources))
    if original is None:
        config["moduleInstances"] = initial_instances or {}
        config["menuElements"] = [{"name": name, "open": True} for name in [TITLE, "Source", "Radio", "Sinks", "Recorder", "Frequency Manager", "Band Plan", "Display"]]
        config["frequency"] = 14074000.0
        if "RTL-SDR Source" in config["moduleInstances"]:
            config["source"] = "RTL-SDR Source"
    assert_host_stopped(binary, profile)
    target = profile / "plugins" / "dx-notebook"
    if target.parent.is_symlink() or target.is_symlink() or any(p.is_symlink() for p in target.rglob("*")):
        raise ValueError("A symlinked plugin payload must be managed manually.")
    target.parent.mkdir(parents=True, exist_ok=True)
    backup = profile / "kondor-backups" / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup.mkdir(parents=True)
    if original is not None:
        (backup / "config.json").write_bytes(original)
    with tempfile.TemporaryDirectory(prefix=".kondor-stage-", dir=target.parent) as temporary:
        stage = Path(temporary) / "payload"
        run("cmake", "--install", build, "--prefix", stage)
        metadata = json.loads((stage / "dx_notebook.host.json").read_text())
        if metadata.get("version") != VERSION or metadata.get("host") != "sdrpp" or metadata.get("platform") != "Linux":
            raise ValueError("Payload host/version metadata does not match this Linux installer. Rebuild the selected target.")
        run(stage / "dx_host_probe", core, stage / "dx_notebook.so", timeout=20)
        for name in ["dx_decoder", "dx_modes_decoder", "dx_wspr_decoder", "dx_js8_decoder"]:
            if (stage / name).exists():
                run(stage / name, "--check-runtime", stdin=subprocess.DEVNULL, timeout=30)
        files = {p.name: digest(p) for p in stage.iterdir() if p.is_file()}
        atomic_json(stage / "manifest.json", {"version": VERSION, "platform": platform.machine(), "host_core": str(core), "host_sha256": digest(core), "files": files})
        config.setdefault("modules", [])
        plugin_path = str(target / "dx_notebook.so")
        config["modules"] = [p for p in config["modules"] if not (isinstance(p, str) and Path(p).name in {"dx_notebook.so", "dx_notebook.dylib"})] + [plugin_path]
        instances = config.setdefault("moduleInstances", {})
        matching = [name for name, value in instances.items() if isinstance(value, dict) and value.get("module") == "advanced_ft8_ft4_dx_decoder"]
        name = matching[0] if matching else TITLE
        instance = instances.setdefault(name, {"module": "advanced_ft8_ft4_dx_decoder", "enabled": True})
        if grid and not instance.get("receiverGrid"):
            instance["receiverGrid"] = grid.upper()
        assert_host_stopped(binary, profile)
        if (config_path.read_bytes() if config_path.exists() else None) != original:
            raise ValueError("Host configuration changed during preflight; rerun after closing the host.")
        had_target = target.exists()
        if had_target:
            os.replace(target, backup / "plugin")
        try:
            os.replace(stage, target)
            atomic_json(config_path, config)
        except BaseException:
            if target.exists():
                os.replace(target, Path(temporary) / "failed-payload")
            if had_target:
                os.replace(backup / "plugin", target)
            if original is not None:
                config_path.write_bytes(original)
            elif config_path.exists():
                config_path.unlink()
            raise
    print(f"Installed Kondor {VERSION}. Backup: {backup}\nReception history and CSV files were not opened or changed.")
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependencies", action="store_true", help="Print the distribution's dependency command and exit")
    parser.add_argument("--install-deps", action="store_true", help="Install build dependencies through the system package manager (sudo)")
    parser.add_argument("--host-source", type=Path, help="Exact source tree matching an existing SDR++ host")
    parser.add_argument("--host-core", type=Path, help="Existing libsdrpp_core.so, required with --host-source")
    parser.add_argument("--host-binary", type=Path, help="Existing sdrpp executable, required with --host-source")
    parser.add_argument("--profile", type=Path, help="Selected host's -r profile directory")
    parser.add_argument("--resources-dir", type=Path, help="Host resources, if not already configured or found beside the host")
    parser.add_argument("--modules-dir", type=Path, help="Host modules, if not already configured or found beside the host")
    parser.add_argument("--grid", default="", help="Receiver Maidenhead locator, e.g. KM69; never replaces an existing grid")
    parser.add_argument("--jobs", type=int, default=default_jobs())
    parser.add_argument("--cache", type=Path, default=Path(os.environ.get("XDG_CACHE_HOME", str(Path.home() / ".cache"))) / "kondor-linux")
    parser.add_argument("--skip-build", type=Path, help="Install an already tested build (still checks the host and helpers)")
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("Run this installer on Linux.")
    deps = dependencies()
    if args.dependencies:
        print(shlex.join(["sudo"] + deps) if deps else "See docs/LINUX.md for required libraries.")
        return
    if args.grid and not re.fullmatch(r"[A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2}(?:[0-9]{2})?)?", args.grid):
        parser.error("Use a 4, 6 or 8 character Maidenhead locator.")
    if args.jobs < 1 or args.jobs > 64:
        parser.error("Use 1 to 64 build jobs.")
    if args.install_deps:
        if not deps:
            parser.error("No package recipe for this distribution. Install dependencies manually.")
        if deps[0] == "apt-get":
            run("sudo", "apt-get", "update")
        run("sudo", *deps)
    for tool in ["git", "cmake", "pkg-config", "g++", "gfortran"]:
        if not shutil.which(tool):
            parser.error(f"Missing {tool}. Run ./install-linux.sh --dependencies or add --install-deps.")
    args.cache.mkdir(parents=True, exist_ok=True)
    with (args.cache / "install.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        existing = args.host_source is not None
        if existing:
            if not args.host_core or not args.host_binary:
                parser.error("Existing hosts require --host-source, --host-core and --host-binary together.")
            source, core, binary = [p.expanduser().resolve(strict=True) for p in [args.host_source, args.host_core, args.host_binary]]
            profile = args.profile or Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))) / "sdrpp"
            config_path = profile.expanduser() / "config.json"
            config = json.loads(config_path.read_text()) if config_path.is_file() else {}
            if not isinstance(config, dict):
                parser.error("The host configuration must be a JSON object.")
            prefix = binary.parent.parent
            resources = args.resources_dir
            if resources is None:
                resources = Path(config["resourcesDirectory"]) if config.get("resourcesDirectory") else prefix / "share/sdrpp"
            modules = args.modules_dir
            if modules is None:
                modules = Path(config["modulesDirectory"]) if config.get("modulesDirectory") else core.parent / "sdrpp/plugins"
            if not resources.is_dir() or not modules.is_dir():
                parser.error("Host resources/modules not found. Use --resources-dir and --modules-dir for this host's actual installation.")
        else:
            if args.host_core or args.host_binary:
                parser.error("Specify --host-source when installing into an existing host.")
            source = repository("https://github.com/AlexandreRouma/SDRPlusPlus.git", HOST_PIN, args.cache / ("sdrpp-" + HOST_PIN[:12]))
            prefix = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "kondor/hosts" / HOST_PIN[:12]
            host_build = args.cache / "host-build"
            run("cmake", "-S", source, "-B", host_build, "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(prefix), "-DOPT_BUILD_AIRSPY_SOURCE=OFF", "-DOPT_BUILD_AIRSPYHF_SOURCE=OFF", "-DOPT_BUILD_HACKRF_SOURCE=OFF", "-DOPT_BUILD_PLUTOSDR_SOURCE=OFF", "-DOPT_BUILD_AUDIO_SOURCE=OFF", "-DOPT_BUILD_DISCORD_PRESENCE=OFF")
            run("cmake", "--build", host_build, "-j", args.jobs)
            run("cmake", "--install", host_build)
            binary, core = prefix / "bin/sdrpp", prefix / "lib/libsdrpp_core.so"
            profile = args.profile or Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))) / "sdrpp-kondor"
            resources, modules = prefix / "share/sdrpp", prefix / "lib/sdrpp/plugins"
        profile = profile.expanduser().resolve()
        assert_host_stopped(binary, profile)
        build = args.skip_build
        if build is None:
            decoder = repository("https://github.com/sannysanoff/SDRPlusPlusBrown.git", DECODER_PIN, args.cache / ("decoder-source-" + DECODER_PIN[:12]))
            staged = args.cache / "decoder-staged"
            run(sys.executable, ROOT / "scripts/stage_sources.py", "--source", decoder, "--destination", staged)
            build = args.cache / "plugin-build"
            run("cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release", "-DDX_HOST=sdrpp", "-DDX_HOST_SOURCE=" + str(source), "-DDX_HOST_CORE=" + str(core), "-DBROWN_SOURCE=" + str(staged))
            run("cmake", "--build", build, "-j", args.jobs)
            run("ctest", "--test-dir", build, "--output-on-failure")
        initial = host_defaults(source, modules) if not (profile / "config.json").exists() else None
        install_payload(build.resolve(), profile, binary, core, resources, modules, args.grid, initial)
        launcher = Path.home() / ".local/bin/kondor-sdrpp"
        launcher.parent.mkdir(parents=True, exist_ok=True)
        data = profile / "dx-notebook"
        launcher.write_text("#!/bin/sh\nexport ADVANCED_DX_DATA_ROOT=" + shlex.quote(str(data)) + "\nexport LD_LIBRARY_PATH=" + shlex.quote(str(core.parent)) + '${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\nexec ' + shlex.quote(str(binary)) + " -r " + shlex.quote(str(profile)) + ' "$@"\n')
        launcher.chmod(0o755)
        desktop = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "applications/kondor-sdrpp.desktop"
        desktop.parent.mkdir(parents=True, exist_ok=True)
        desktop.write_text('[Desktop Entry]\nType=Application\nName=SDR++ with Kondor\nComment=Weak signal reception and DX mapping\nExec="' + str(launcher).replace('"', '\\"') + '"\nTerminal=false\nCategories=HamRadio;Science;\n')
        print(f"Launch SDR++ with Kondor from your application menu, or run {launcher}\nSet receiver grid in Kondor Settings > General if not provided. Radio-device permissions remain managed by your host/distribution.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Installation stopped: {error}", file=sys.stderr)
        sys.exit(1)
