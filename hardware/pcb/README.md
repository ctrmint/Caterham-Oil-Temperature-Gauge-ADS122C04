# Interface PCBs

Each board design has its own folder. The folder holds everything for that board: the design and its generator, the build script, the renders, the fab files and the parts lists.

| Board | Status | What it is |
|---|---|---|
| [surface-mount-v1](surface-mount-v1/README.md) | Designed: zero ERC and zero DRC findings. Not built or tested yet. | 39 × 23 mm, two layers, surface-mount parts fitted by the board house. You hand-fit Rp and the two connectors. It plugs onto the display's case socket. [Parts list](surface-mount-v1/BOM.md). |

The circuit they implement is in [../WIRING.md](../WIRING.md).
