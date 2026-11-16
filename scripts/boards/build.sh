#!/usr/bin/env bash
# Rebuild both options' boards and parts lists from scripts/boards/, check them, and write every output into
# hardware/option-a/ and hardware/option-b/:
#   veroboard/  the stripboard layout drawing, its assembly sheet and parts list
#   pcb/        the KiCad project (libraries, schematic, board, DRC rules), renders, Gerbers + drill, parts list
#   BOM.md      the option's parts list for every build, and outputs.sha256
# Fails on any stripboard layout problem, any ERC or DRC finding, or a schematic that doesn't match the circuit.
#
# KiCad 10.0.8 runs in its official container (podman or docker), so every machine produces the same files.
# Set KICAD_LOCAL=1 to use a local KiCad 10.0.8 (kicad-cli and its Python pcbnew module) instead.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE=docker.io/kicad/kicad:10.0.8-full

if [ "${INSIDE_KICAD:-}" != 1 ]; then
  if [ "${KICAD_LOCAL:-}" = 1 ]; then
    INSIDE_KICAD=1 exec "$0" "$@"
  fi
  if command -v podman >/dev/null; then
    exec podman run --rm --userns=keep-id --user "$(id -u):$(id -g)" --security-opt label=disable \
      -e HOME=/tmp -e INSIDE_KICAD=1 -v "$ROOT":/work -w /work "$IMAGE" bash scripts/boards/build.sh "$@"
  elif command -v docker >/dev/null; then
    exec docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -e INSIDE_KICAD=1 \
      -v "$ROOT":/work -w /work "$IMAGE" bash scripts/boards/build.sh "$@"
  fi
  echo "build.sh: needs podman or docker for $IMAGE, or KICAD_LOCAL=1 with KiCad 10.0.8" >&2
  exit 1
fi

cd "$ROOT"
[ "$(kicad-cli version)" = 10.0.8 ] || { echo "build.sh: needs KiCad 10.0.8, found $(kicad-cli version)" >&2; exit 1; }
G=scripts/boards
export PYTHONDONTWRITEBYTECODE=1
export KICAD10_3DMODEL_DIR="${KICAD10_3DMODEL_DIR:-/usr/share/kicad/3dmodels}"
CFG="$HOME/.config/kicad/10.0"
if [ ! -f "$CFG/fp-lib-table" ]; then
  mkdir -p "$CFG"
  cp /usr/share/kicad/template/sym-lib-table /usr/share/kicad/template/fp-lib-table "$CFG/"
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "== stripboards"
python3 "$G/vero.py"

for o in a b; do
  D=hardware/option-$o
  N=option-$o
  P=$D/pcb
  echo "== option $o: stripboard"
  mkdir -p "$D/veroboard"
  python3 "$G/vero_draw.py" "$o" "$D/veroboard/layout.svg"
  python3 "$G/vero_outputs.py" "$o" "$D/veroboard"

  echo "== option $o: PCB libraries"
  export BOARD_OPTION=$o
  mkdir -p "$P/3d" "$P/renders" "$P/fab"
  python3 "$G/pcb_wrl.py" "$P/3d"
  rm -rf "$P/footprints.pretty"
  python3 "$G/pcb_footprints.py" "$P"
  printf '(sym_lib_table\n  (version 7)\n  (lib (name "gauge")(type "KiCad")(uri "${KIPRJMOD}/gauge.kicad_sym")(options "")(descr "ADS122C04 module symbol"))\n)\n' > "$P/sym-lib-table"
  printf '(fp_lib_table\n  (version 7)\n  (lib (name "gauge")(type "KiCad")(uri "${KIPRJMOD}/footprints.pretty")(options "")(descr "the board'"'"'s footprints"))\n)\n' > "$P/fp-lib-table"

  # A project file, so kicad-cli uses the project's library tables and its custom DRC rules (pcb_board.py writes
  # them). pcbnew fills in the board settings when it saves the board.
  [ -f "$P/$N.kicad_pro" ] || printf '{\n  "meta": {\n    "filename": "%s.kicad_pro",\n    "version": 3\n  }\n}\n' "$N" > "$P/$N.kicad_pro"

  echo "== option $o: schematic"
  python3 "$G/pcb_sch.py" "$P/$N.kicad_sch"
  kicad-cli sch upgrade "$P/$N.kicad_sch" >/dev/null
  kicad-cli sch erc --severity-all --exit-code-violations -o "$TMP/erc.rpt" "$P/$N.kicad_sch" >/dev/null \
    || { cat "$TMP/erc.rpt"; exit 1; }
  kicad-cli sch export netlist --format kicadsexpr -o "$TMP/net.net" "$P/$N.kicad_sch" >/dev/null
  python3 "$G/pcb_check_netlist.py" "$TMP/net.net"

  echo "== option $o: board"
  python3 "$G/pcb_board.py" "$TMP/net.net" "$P/$N.kicad_pcb"
  kicad-cli pcb drc --schematic-parity --severity-all --exit-code-violations \
    -o "$TMP/drc.rpt" "$P/$N.kicad_pcb" >/dev/null || { cat "$TMP/drc.rpt"; exit 1; }
  grep -E '^\*\* Found' "$TMP/drc.rpt"

  echo "== option $o: renders"
  # The ray tracer isn't bit-for-bit repeatable, so only re-render when the board changed.
  STAMP="$(sha256sum "$P/$N.kicad_pcb" "$P"/3d/*.wrl | sha256sum | cut -c1-64)"
  if [ "$(cat "$P/renders/.board-sha256" 2>/dev/null)" != "$STAMP" ] || [ ! -f "$P/renders/iso.png" ]; then
    R=(kicad-cli pcb render --quality high --width 1600 --height 1000 --background opaque)
    "${R[@]}" --side top -o "$P/renders/top.png" "$P/$N.kicad_pcb" >/dev/null
    "${R[@]}" --side top --rotate '-50,0,-25' --zoom 1.05 --perspective -o "$P/renders/iso.png" "$P/$N.kicad_pcb" >/dev/null
    echo "$STAMP" > "$P/renders/.board-sha256"
  fi
  python3 "$G/pcb_review_plot.py" "$P/$N.kicad_pcb" "$P/renders/placement.svg"
  rm -rf "$TMP/sch" && mkdir -p "$TMP/sch"
  kicad-cli sch export svg -o "$TMP/sch" "$P/$N.kicad_sch" >/dev/null
  sed -E 's/(created as [^ ]+ date )[0-9T:-]+/\1(see title block)/' "$TMP/sch/$N.svg" > "$P/renders/schematic.svg"

  echo "== option $o: fab"
  rm -rf "$TMP/plot" && mkdir -p "$TMP/plot"
  kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,Edge.Cuts \
    -o "$TMP/plot/" "$P/$N.kicad_pcb" >/dev/null
  kicad-cli pcb export drill --format excellon --excellon-units mm -o "$TMP/plot/" "$P/$N.kicad_pcb" >/dev/null
  python3 "$G/outputs.py" gerbers "$TMP/plot" "$P/fab/$N-gerbers.zip"
  rm -f "$P/$N.kicad_prl" "$P"/*.lck "$P/fp-info-cache"

  echo "== option $o: parts lists and manifest"
  python3 "$G/outputs.py" bom "$o" "$D"
  python3 "$G/outputs.py" manifest "$D"
done
echo "== done"
