#!/usr/bin/env bash
# Rebuild the Empty Slot channel WADs, for Wii and vWii. Pass a System Menu content folder to
# use something other than Dolphin's NAND.
set -euo pipefail

cd "$(dirname "$0")"
PY="${PY:-$PWD/.venv/bin/python}"

if [ ! -x "$PY" ]; then
    python3 -m venv .venv
fi
"$PY" -c 'import PIL, Crypto, libWiiPy' 2>/dev/null || \
    "$PY" -m pip install -q pillow pycryptodome libWiiPy

mkdir -p out

echo "==> forwarder"
make -s -C ../forwarder

echo "==> System Menu slot layout + donor"
"$PY" tools/extract.py "$@"

echo "==> icon.bin + banner.bin"
"$PY" tools/build_layouts.py

echo "==> sound.bin"
"$PY" tools/build_sound.py

echo "==> WAD"
"$PY" tools/build_wad.py

echo "==> verify"
"$PY" tools/verify.py
