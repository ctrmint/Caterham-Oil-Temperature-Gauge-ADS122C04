#!/usr/bin/env bash
# Rebuild through-hole-usb-v1 (this folder) from generator/, check it, and write every output:
# the board's footprint and symbol libraries, schematic, board, ERC/DRC (with schematic parity), renders,
# Gerbers + drill, the box drawing, the parts lists and outputs.sha256. Fails on any ERC or DRC finding.
#
# KiCad 10.0.8 runs in its official container (podman or docker), so every machine produces the same files.
# Set KICAD_LOCAL=1 to use a local KiCad 10.0.8 (kicad-cli and its Python pcbnew module) instead.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
IMAGE=docker.io/kicad/kicad:10.0.8-full

if [ "${INSIDE_KICAD:-}" != 1 ]; then
  if [ "${KICAD_LOCAL:-}" = 1 ]; then
    INSIDE_KICAD=1 exec "$0" "$@"
  fi
  if command -v podman >/dev/null; then
    exec podman run --rm --userns=keep-id --user "$(id -u):$(id -g)" --security-opt label=disable \
      -e HOME=/tmp -e INSIDE_KICAD=1 -v "$HERE":/work -w /work "$IMAGE" bash build.sh
  elif command -v docker >/dev/null; then
    exec docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -e INSIDE_KICAD=1 \
      -v "$HERE":/work -w /work "$IMAGE" bash build.sh
  fi
  echo "build.sh: needs podman or docker for $IMAGE, or KICAD_LOCAL=1 with KiCad 10.0.8" >&2
  exit 1
fi

cd "$HERE"
[ "$(kicad-cli version)" = 10.0.8 ] || { echo "build.sh: needs KiCad 10.0.8, found $(kicad-cli version)" >&2; exit 1; }
G=generator
N=through-hole-usb-v1
export KICAD10_3DMODEL_DIR="${KICAD10_3DMODEL_DIR:-/usr/share/kicad/3dmodels}"
CFG="$HOME/.config/kicad/10.0"
if [ ! -f "$CFG/fp-lib-table" ]; then
  mkdir -p "$CFG"
  cp /usr/share/kicad/template/sym-lib-table /usr/share/kicad/template/fp-lib-table "$CFG/"
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP" "$G/__pycache__" "$N.kicad_prl"' EXIT

echo "== libraries"
rm -rf footprints.pretty
python3 "$G/gen_wrl.py" 3d
python3 "$G/gen_footprints.py" .
printf '(sym_lib_table\n  (version 7)\n  (lib (name "%s")(type "KiCad")(uri "${KIPRJMOD}/%s.kicad_sym")(options "")(descr "through-hole-usb-v1 module symbol"))\n)\n' "$N" "$N" > sym-lib-table
printf '(fp_lib_table\n  (version 7)\n  (lib (name "%s")(type "KiCad")(uri "${KIPRJMOD}/footprints.pretty")(options "")(descr "through-hole-usb-v1 single-sided footprints"))\n)\n' "$N" > fp-lib-table

echo "== schematic"
python3 "$G/gen_sch.py" "$N.kicad_sch"
kicad-cli sch upgrade "$N.kicad_sch" >/dev/null
kicad-cli sch erc --severity-all --exit-code-violations -o "$TMP/erc.rpt" "$N.kicad_sch" >/dev/null \
  || { cat "$TMP/erc.rpt"; exit 1; }
kicad-cli sch export netlist --format kicadsexpr -o "$TMP/net.net" "$N.kicad_sch" >/dev/null
python3 "$G/check_netlist.py" "$TMP/net.net"

echo "== board"
python3 "$G/gen_pcb.py" "$TMP/net.net" "$N.kicad_pcb"
kicad-cli pcb drc --schematic-parity --severity-all --refill-zones --save-board --exit-code-violations \
  -o "$TMP/drc.rpt" "$N.kicad_pcb" >/dev/null || { cat "$TMP/drc.rpt"; exit 1; }
grep -E '^\*\* Found' "$TMP/drc.rpt"

echo "== renders"
mkdir -p renders fab box "$TMP/plot" "$TMP/sch"
# The ray tracer isn't bit-for-bit repeatable, so only re-render when the board changed.
STAMP="$(sha256sum "$N.kicad_pcb" 3d/*.wrl | sha256sum | cut -c1-64)"
if [ "$(cat renders/.board-sha256 2>/dev/null)" != "$STAMP" ] || [ ! -f renders/iso.png ]; then
  R=(kicad-cli pcb render --quality high --width 1600 --height 1200 --background opaque)
  "${R[@]}" --side top -o renders/top.png "$N.kicad_pcb" >/dev/null
  "${R[@]}" --side bottom -o renders/bottom.png "$N.kicad_pcb" >/dev/null
  "${R[@]}" --side top --rotate '-50,0,-25' --zoom 1.05 --perspective -o renders/iso.png "$N.kicad_pcb" >/dev/null
  echo "$STAMP" > renders/.board-sha256
fi
python3 "$G/review_plot.py" "$N.kicad_pcb" renders/placement.svg
kicad-cli sch export svg -o "$TMP/sch" "$N.kicad_sch" >/dev/null
sed -E 's/(created as [^ ]+ date )[0-9T:-]+/\1(see title block)/' "$TMP/sch/$N.svg" > renders/schematic.svg
python3 "$G/box_drawing.py" "$N.kicad_pcb" "box/$N-box.svg"

echo "== fab"
# One copper layer: bottom copper, its mask, the component-side legend and the outline.
kicad-cli pcb export gerbers --layers B.Cu,B.Mask,F.Silkscreen,Edge.Cuts -o "$TMP/plot/" "$N.kicad_pcb" >/dev/null
kicad-cli pcb export drill --format excellon --excellon-units mm -o "$TMP/plot/" "$N.kicad_pcb" >/dev/null
python3 "$G/outputs.py" gerbers "$TMP/plot" "fab/$N-gerbers.zip"
python3 "$G/outputs.py" bom .

rm -rf "$G/__pycache__" "$N.kicad_prl"
python3 "$G/outputs.py" manifest .
echo "== done: $N"
