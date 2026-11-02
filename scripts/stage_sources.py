"""Stage the exact installed Brown ABI without modifying its checkout."""
import argparse
from pathlib import Path
import subprocess
import tarfile
import io

PIN = "679f48deee5299a199d15d30e6e785fac6054896"
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    archive = subprocess.check_output(["git", "-C", args.source, "archive", PIN,
                                       "license", "core", "decoder_modules/ft8_decoder", "decoder_modules/radio"])
    with tarfile.open(fileobj=io.BytesIO(archive)) as files:
        files.extractall(destination, filter="data")
    # Resource-only corrections. Decoder algorithms and sensitivity settings stay pinned.
    decoder = destination / "decoder_modules/ft8_decoder/src"
    path = decoder / "ft8_etc/decoderpom.h"
    text = path.read_text()
    old = "~F2a() {\n    }"
    assert text.count(old) == 1
    path.write_text(text.replace(old, "~F2a() {\n        DestroyPlansAll(true);\n    }"))
    path = decoder / "fftw_mshv_plug_original.h"
    text = path.read_text()
    old = "if (!allPlans[i].alive) {\n                return i;"
    assert text.count(old) == 1
    text = text.replace(old, "if (!allPlans[i].alive) {\n                allPlans[i] = FFT_PLAN_IMPL{};\n                allPlans[i].alive = true;\n                return i;")
    old = "p.alive = false;"
    assert text.count(old) == 1
    text = text.replace(old, "p.alive = false;\n        p.plan = nullptr;")
    path.write_text(text)
    (destination / "source-pin.txt").write_text(PIN + "\n")
    print("Pinned ABI and decoder staged; FFT plan lifetime fixes applied.")


if __name__ == "__main__":
    main()
