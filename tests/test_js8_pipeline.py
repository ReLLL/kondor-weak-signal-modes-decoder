"""Isolated real JS8 audio through Engine, SQLite, map/table queries and CSV."""
import csv
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import shlex
import shutil


def main():
    pipeline, helper, reference, mock = [Path(p).resolve() for p in sys.argv[1:]]
    with tempfile.TemporaryDirectory(prefix="dx-js8-pipeline-") as temporary:
        root = Path(temporary)
        (root / ".js8-test-owned").write_text("synthetic JS8 reception fixture; disposable")
        # Preserve the isolated helper's diagnostic output when a pipeline
        # assertion fails; never enable logging in the normal receiver profile.
        launcher = root / "dx_js8_decoder"
        frozen = root / "js8-helper-test"
        shutil.copy2(helper, frozen)
        for library in helper.parent.glob("libkondor_*.dylib"):
            shutil.copy2(library, root / library.name)
        launcher.write_text("#!/bin/sh\nexec " + shlex.quote(str(frozen)) + " 2>>" + shlex.quote(str(root / "helper-stderr.txt")) + "\n")
        launcher.chmod(0o755)
        (root / "dx_mock_valid_small").symlink_to(mock)
        metadata = {"period": 15, "frames": []}
        for text in ("HB JO33", "K1ABC HELLO THIS IS A LONG MESSAGE TESTING THE JS8 RECEPTION PIPELINE"):
            encoded = subprocess.check_output([str(reference), "15", "DL8OH", "JO33", text], cwd=root)
            size, = struct.unpack("<I", encoded[:4]);header = json.loads(encoded[4:4 + size]);pcm = encoded[4 + size:]
            block = 15 * 12000 * 2
            for i, frame in enumerate(header["frames"]):
                (root / f"frame-{len(metadata['frames'])}.pcm").write_bytes(pcm[i * block:(i + 1) * block])
                metadata["frames"].append(frame)
        (root / "frames.json").write_text(json.dumps(metadata))
        result = subprocess.run([str(pipeline), str(root)], timeout=120)
        if result.returncode:
            print((root / "helper-stderr.txt").read_text(), file=sys.stderr)
            result.check_returncode()
        with (root / "profile/receptions.csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        js8 = [row for row in rows if row["Mode"] == "JS8"]
        assert js8 and all(row["Call"] == "DL8OH" for row in js8)
        assert "HELLO THIS IS A LONG MESSAGE TESTING THE JS8 RECEPTION PIPELINE" in js8[-1]["Message"]
        assert json.loads(js8[-1]["Raw"])[-1]["snr"] == int(js8[-1]["SNRdB"])
        assert {row["Mode"] for row in rows} == {"FT8", "JS8"}
        print("PASS reception CSV retains JS8 frame times/SNR and complete text alongside FT8")


if __name__ == "__main__":
    main()
