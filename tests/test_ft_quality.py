"""Deterministic FT8/FT4 confidence/IPC checks; no receiver profile is opened."""
import array
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile


def request(helper, mode, pcm, folder, depth=None):
    header = {"id": 1, "mode": mode, "utc_ms": 1800000000000, "samples": len(pcm) // 2}
    if depth is not None:
        header["decode_depth"] = depth
    body = json.dumps(header).encode()
    result = subprocess.run([str(helper)], input=struct.pack("<I", len(body)) + body + pcm,
                            cwd=folder, capture_output=True, check=True, timeout=30)
    size, = struct.unpack("<I", result.stdout[:4])
    assert size == len(result.stdout) - 4
    response = json.loads(result.stdout[4:])
    assert response["malformed"] == 0, response
    return response["results"]


def main():
    helper, fixture = [Path(value).resolve() for value in sys.argv[1:]]
    with tempfile.TemporaryDirectory(prefix="dx-ft-quality-") as temporary:
        root = Path(temporary)
        for mode in ("FT8", "FT4"):
            for message in ("CQ K1ABC FN42", "S93 YO2MHF KN"):
                path = root / "fixture.pcm"
                subprocess.run([str(fixture), mode, str(path), message], check=True, cwd=root, timeout=15)
                for depth in (1, 2, 3):
                    rows = request(helper, mode, path.read_bytes(), root, depth)
                    matches = [row for row in rows if row["message"].strip() == message]
                    assert matches, (mode, depth, message, rows)
                    assert all("decoder_flags" in row and "decoder_quality" in row and
                               "decoder_suspect" in row for row in rows)
                    print("PASS", mode, "depth", depth, message, flush=True)
            for seed in range(3):
                rng = random.Random(20261102 + seed)
                pcm = array.array("h", (round(rng.gauss(0, 1000)) for _ in range(180000 if mode == "FT8" else 90000)))
                if sys.byteorder != "little":
                    pcm.byteswap()
                rows = request(helper, mode, pcm.tobytes(), root)
                assert not rows, (mode, seed, rows)
            print("PASS", mode, "three deterministic noise slots at default depth", flush=True)
        for changes in ({"decode_depth": 0}, {"decode_depth": 4}, {"decode_depth": "Deep"},
                        {"id": False}, {"utc_ms": "invalid"}, {"mode": 8}):
            header = {"id": 1, "mode": "FT8", "utc_ms": 1800000000000, "samples": 180000, **changes}
            body = json.dumps(header).encode()
            response = subprocess.run([str(helper)], input=struct.pack("<I", len(body)) + body,
                                      cwd=root, capture_output=True, timeout=5)
            assert response.returncode == 2, (changes, response.returncode)
    assert not root.exists()
    print("FT8/FT4 structured/free text, all depths, noise and invalid IPC passed; fixtures removed")


if __name__ == "__main__":
    main()
