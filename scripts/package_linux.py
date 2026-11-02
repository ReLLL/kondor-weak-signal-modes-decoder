#!/usr/bin/env python3
"""Package the native Linux source installer, without host binaries or user data."""
import argparse
import hashlib
from pathlib import Path
import re
import tarfile
from package_release import anonymous_owner, source_files

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    version = re.search(r'project\([^\n]+ VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text())[1]
    name = 'kondor-weak-signal-modes-decoder-' + version
    args.output_dir.mkdir(parents=True, exist_ok=True)
    archive = args.output_dir / (name + '-linux-source-installer.tar.gz')
    if archive.exists():
        raise SystemExit('Output already exists; choose a fresh release directory')
    files = {path for path in source_files() if not path.relative_to(ROOT).parts[0].startswith('build')}
    # Include the exact pinned decoder/host headers used by the build. The
    # installer still builds a separate matching Linux SDR++ when requested.
    for relative in ('license', 'source-pin.txt', 'core', 'decoder_modules/ft8_decoder', 'decoder_modules/radio'):
        path = ROOT / 'build' / relative
        files.update(p for p in path.rglob('*') if p.is_file()) if path.is_dir() else files.add(path)
    if not all(path.is_file() and not path.is_symlink() for path in files):
        raise ValueError('Missing or symlinked source input')
    with tarfile.open(archive, 'w:gz') as output:
        for path in sorted(files):
            relative = path.relative_to(ROOT)
            if path.name == 'AGENTS.md' or {'.git', '.codex', '__pycache__'}.intersection(relative.parts):
                raise ValueError('Private input in Linux source package')
            output.add(path, arcname=Path(name) / relative, filter=anonymous_owner, recursive=False)
    print(archive)
    print(hashlib.sha256(archive.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
