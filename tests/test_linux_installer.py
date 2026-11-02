"""Installer transactions never touch reception data, including on failure."""
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import subprocess
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("linux_installer", ROOT / "linux/install.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallTransactions(unittest.TestCase):
    def setUp(self):
        self.guard = patch.object(installer, "assert_host_stopped")
        self.guard.start()
        self.temp = tempfile.TemporaryDirectory(prefix="kondor-installer-test-")
        self.root = Path(self.temp.name)
        self.profile = self.root / "profile"
        self.profile.mkdir()
        self.target = self.profile / "plugins/dx-notebook"
        self.target.mkdir(parents=True)
        (self.target / "old").write_bytes(b"previous plugin")
        self.core = self.root / "libsdrpp_core.so"
        self.core.write_bytes(b"test-only core")
        self.binary = self.root / "sdrpp-test-only"
        self.binary.write_bytes(b"test-only host")
        self.config = self.profile / "config.json"
        self.original = {"modules": ["/existing/radio.so"], "moduleInstances": {"Existing Kondor": {"module": "advanced_ft8_ft4_dx_decoder", "enabled": False, "receiverGrid": "FN42"}}, "untouched": {"value": 7}}
        self.config.write_text(json.dumps(self.original))
        self.data = self.profile / "dx-notebook"
        self.data.mkdir()
        for name in ["history.sqlite3", "history.sqlite3-wal", "receptions.csv"]:
            (self.data / name).write_bytes((name + " unchanged").encode())
        self.before = {p.name: p.read_bytes() for p in self.data.iterdir()}

    def tearDown(self):
        self.guard.stop()
        self.assertEqual(self.before, {p.name: p.read_bytes() for p in self.data.iterdir()})
        self.temp.cleanup()

    def command(self, *args, **kwargs):
        if args[:2] == ("cmake", "--install"):
            stage = Path(args[-1])
            stage.mkdir()
            for name in ["dx_notebook.so", "dx_host_probe", "dx_decoder"]:
                (stage / name).write_bytes(b"test-only staged artifact")
            (stage / "dx_notebook.host.json").write_text(json.dumps({"host": "sdrpp", "version": installer.VERSION, "platform": "Linux"}))

    def install(self):
        return installer.install_payload(self.root, self.profile, self.binary, self.core, self.root, self.root, "KM69")

    def test_upgrade_preserves_settings_disabled_state_grid_and_history(self):
        with patch.object(installer, "run", self.command):
            self.install()
        saved = json.loads(self.config.read_text())
        self.assertEqual(saved["moduleInstances"], self.original["moduleInstances"])
        self.assertEqual(saved["untouched"], self.original["untouched"])
        self.assertIn("/existing/radio.so", saved["modules"])
        self.assertTrue((self.target / "dx_notebook.so").is_file())
        self.assertEqual(len(list((self.profile / "kondor-backups").glob("*/plugin/old"))), 1)

    def test_failed_preflight_keeps_old_plugin_and_config(self):
        def fail(*args, **kwargs):
            self.command(*args, **kwargs)
            if str(args[0]).endswith("dx_host_probe"):
                raise ValueError("intentional incompatible test host")
        with patch.object(installer, "run", fail), self.assertRaises(ValueError):
            self.install()
        self.assertTrue((self.target / "old").is_file())
        self.assertEqual(json.loads(self.config.read_text()), self.original)

    def test_wrong_version_payload_is_not_installed(self):
        def wrong(*args, **kwargs):
            self.command(*args, **kwargs)
            if args[:2] == ("cmake", "--install"):
                (Path(args[-1]) / "dx_notebook.host.json").write_text('{"host":"sdrpp","platform":"Linux","version":"0.0.1"}')
        with patch.object(installer, "run", wrong), self.assertRaisesRegex(ValueError, "host/version"):
            self.install()
        self.assertTrue((self.target / "old").is_file())
        self.assertEqual(json.loads(self.config.read_text()), self.original)

    def test_config_write_failure_rolls_payload_back(self):
        write = installer.atomic_json
        def fail(path, value):
            if path.resolve() == self.config.resolve():
                raise OSError("intentional write failure")
            write(path, value)
        with patch.object(installer, "run", self.command), patch.object(installer, "atomic_json", fail), self.assertRaises(OSError):
            self.install()
        self.assertTrue((self.target / "old").is_file())
        self.assertEqual(json.loads(self.config.read_text()), self.original)

    def test_configuration_changed_during_build_is_not_overwritten(self):
        def change(*args, **kwargs):
            self.command(*args, **kwargs)
            if str(args[0]).endswith("dx_host_probe"):
                self.config.write_text('{"changed": true}')
        with patch.object(installer, "run", change), self.assertRaisesRegex(ValueError, "changed during"):
            self.install()
        self.assertTrue((self.target / "old").is_file())
        self.assertEqual(json.loads(self.config.read_text()), {"changed": True})

    def test_symlinked_payload_refused(self):
        (self.target / "link").symlink_to(self.core)
        with self.assertRaisesRegex(ValueError, "symlinked"):
            self.install()

    def test_fresh_profile_keeps_host_receivers_and_adds_kondor(self):
        self.config.unlink()
        initial = {"Radio": {"module": "radio", "enabled": True}, "RTL-SDR Source": {"module": "rtl_sdr_source", "enabled": True}}
        with patch.object(installer, "run", self.command):
            installer.install_payload(self.root, self.profile, self.binary, self.core, self.root, self.root, "KM69", initial)
        saved = json.loads(self.config.read_text())
        for name in initial:
            self.assertEqual(saved["moduleInstances"][name], initial[name])
        self.assertEqual(saved["moduleInstances"][installer.TITLE]["receiverGrid"], "KM69")
        self.assertEqual(saved["source"], "RTL-SDR Source")
        self.assertTrue(all(isinstance(item, dict) and "name" in item for item in saved["menuElements"]))

    def test_same_profile_concurrent_installation_refused(self):
        with (self.profile / ".kondor-install.lock").open("a") as lock:
            installer.fcntl.flock(lock, installer.fcntl.LOCK_EX | installer.fcntl.LOCK_NB)
            with self.assertRaises(BlockingIOError):
                self.install()
        self.assertEqual(json.loads(self.config.read_text()), self.original)

    @unittest.skipUnless(sys.platform == "linux", "Linux process inspection")
    def test_running_host_is_refused_without_stopping_it(self):
        self.guard.stop()
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)", str(self.profile)])
        try:
            with self.assertRaisesRegex(ValueError, "Close the selected"):
                installer.assert_host_stopped(Path(sys.executable), self.profile)
            self.assertIsNone(child.poll())
        finally:
            child.terminate()
            child.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
