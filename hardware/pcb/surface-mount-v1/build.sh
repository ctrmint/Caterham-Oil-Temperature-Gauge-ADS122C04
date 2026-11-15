#!/usr/bin/env bash
# Rebuild the interface PCB (hardware/pcb) from hardware/pcb/generator, check it, and write every output:
# schematic, board, ERC/DRC (with schematic parity), renders, Gerbers + drill, placement file, BOMs and
# hardware/pcb/outputs.sha256. Fails on any ERC or DRC finding.
#
# KiCad 10.0.8 runs in its official container (podman or docker), so every machine produces the same files.
# Set KICAD_LOCAL=1 to use a local KiCad 10.0.8 (kicad-cli and its Python pcbnew module) instead.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE=docker.io/kicad/kicad:10.0.8-full

if [ "${INSIDE_KICAD:-}" != 1 ]; then
  if [ "${KICAD_LOCAL:-}" = 1 ]; then
    INSIDE_KICAD=1 exec "$0" "$@"
  fi
  if command -v podman >/dev/null; then
    exec podman run --rm --userns=keep-id --user "$(id -u):$(id -g)" --security-opt label=disable \
      -e HOME=/tmp -e INSIDE_KICAD=1 -v "$ROOT":/work -w /work "$IMAGE" bash scripts/pcb_build.sh
  elif command -v docker >/dev/null; then
    exec docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -e INSIDE_KICAD=1 \
      -v "$ROOT":/work -w /work "$IMAGE" bash scripts/pcb_build.sh
  fi
  echo "pcb_build.sh: needs podman or docker for $IMAGE, or KICAD_LOCAL=1 with KiCad 10.0.8" >&2
  exit 1
fi

cd "$ROOT"
[ "$(kicad-cli version)" = 10.0.8 ] || { echo "pcb_build.sh: needs KiCad 10.0.8, found $(kicad-cli version)" >&2; exit 1; }
P=hardware/pcb
G=$P/generator
N=oil-gauge-interface
export KICAD10_3DMODEL_DIR="${KICAD10_3DMODEL_DIR:-/usr/share/kicad/3dmodels}"
CFG="$HOME/.config/kicad/10.0"
if [ ! -f "$CFG/fp-lib-table" ]; then
  mkdir -p "$CFG"
  cp /usr/share/kicad/template/sym-lib-table /usr/share/kicad/template/fp-lib-table "$CFG/"
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP" "$G/__pycache__" "$P/$N.kicad_prl"' EXIT

echo "== schematic"
mkdir -p "$P/3d"
python3 "$G/gen_wrl.py" "$P/3d/Molex_43650-0415_envelope.wrl"
python3 "$G/gen_sch.py" "$P/$N.kicad_sch"
kicad-cli sch upgrade "$P/$N.kicad_sch" >/dev/null
kicad-cli sch erc --severity-all --exit-code-violations -o "$TMP/erc.rpt" "$P/$N.kicad_sch" >/dev/null \
  || { cat "$TMP/erc.rpt"; exit 1; }
kicad-cli sch export netlist --format kicadsexpr -o "$TMP/net.net" "$P/$N.kicad_sch" >/dev/null
python3 "$G/check_netlist.py" "$TMP/net.net"

echo "== board"
python3 "$G/gen_pcb.py" "$TMP/net.net" "$P/$N.kicad_pcb"
kicad-cli pcb drc --schematic-parity --severity-all --refill-zones --save-board --exit-code-violations \
  -o "$TMP/drc.rpt" "$P/$N.kicad_pcb" >/dev/null || { cat "$TMP/drc.rpt"; exit 1; }
grep -E '^\*\* Found' "$TMP/drc.rpt"

echo "== renders"
mkdir -p "$P/renders" "$P/fab" "$TMP/plot" "$TMP/sch"
# The ray tracer isn't bit-for-bit repeatable, so only re-render when the board or its 3D model changed.
STAMP="$(sha256sum "$P/$N.kicad_pcb" "$P/3d/"*.wrl | sha256sum | cut -c1-64)"
if [ "$(cat "$P/renders/.board-sha256" 2>/dev/null)" != "$STAMP" ] || [ ! -f "$P/renders/iso.png" ]; then
  R=(kicad-cli pcb render --quality high --width 1600 --height 1200 --background opaque)
  "${R[@]}" --side top -o "$P/renders/top.png" "$P/$N.kicad_pcb" >/dev/null
  "${R[@]}" --side bottom -o "$P/renders/bottom.png" "$P/$N.kicad_pcb" >/dev/null
  "${R[@]}" --side top --rotate '-55,0,-20' --zoom 1.1 --perspective -o "$P/renders/iso.png" "$P/$N.kicad_pcb" >/dev/null
  echo "$STAMP" > "$P/renders/.board-sha256"
fi
python3 "$G/review_plot.py" "$P/$N.kicad_pcb" "$P/renders/placement.svg"
kicad-cli sch export svg -o "$TMP/sch" "$P/$N.kicad_sch" >/dev/null
sed -E 's/(created as [^ ]+ date )[0-9T:-]+/\1(see title block)/' "$TMP/sch/$N.svg" > "$P/renders/schematic.svg"

echo "== fab"
kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts \
  --subtract-soldermask -o "$TMP/plot/" "$P/$N.kicad_pcb" >/dev/null
kicad-cli pcb export drill --format excellon --excellon-separate-th --excellon-units mm -o "$TMP/plot/" \
  "$P/$N.kicad_pcb" >/dev/null
python3 "$G/outputs.py" gerbers "$TMP/plot" "$P/fab/$N-gerbers.zip"
kicad-cli pcb export pos --format csv --units mm --side both -o "$TMP/pos.csv" "$P/$N.kicad_pcb" >/dev/null
python3 "$G/outputs.py" cpl "$TMP/pos.csv" "$P"
python3 "$G/outputs.py" bom "$P"

rm -rf "$G/__pycache__" "$P/$N.kicad_prl"
python3 "$G/outputs.py" manifest .
echo "== done: $P"
