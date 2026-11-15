#!/usr/bin/env bash
# Rebuild veroboard-usb-v1 (this folder) from generator/: check the layout against the circuit and the parts'
# sizes, then write the layout drawing, the parts lists, the assembly sheet and outputs.sha256.
# Fails on any layout problem. Needs only Python 3 (stdlib).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
G=generator
trap 'rm -rf "$G/__pycache__"' EXIT

echo "== check"
python3 "$G/vero.py"

echo "== outputs"
mkdir -p renders
python3 "$G/draw.py" renders/layout.svg
python3 "$G/outputs.py" bom .
python3 "$G/outputs.py" assembly .

rm -rf "$G/__pycache__"
python3 "$G/outputs.py" manifest .
echo "== done: veroboard-usb-v1"
