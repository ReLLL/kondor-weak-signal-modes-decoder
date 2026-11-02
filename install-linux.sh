#!/usr/bin/env bash
set -euo pipefail
if [[ "$(uname -s)" != Linux ]]; then
  printf '%s\n' 'This installer runs on Linux. See README.md for macOS and Windows.' >&2
  exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
  printf '%s\n' 'Install Python 3 using your distribution package manager, then run this command again.' >&2
  exit 1
fi
exec python3 "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/linux/install.py" "$@"
