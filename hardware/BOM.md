# Hardware BOM

Each front-end option has its own complete parts list. Each list covers all three ways to build it: without a board, on stripboard, or on the PCB.

| Option | Front end | Parts list |
|---|---|---|
| **[A, minimal](option-a/README.md)** | Rref, R1 and R2 on the ADC module's pins | [option-a/BOM.md](option-a/BOM.md) |
| **[B, filtered](option-b/README.md)** (recommended for the car) | Option A plus C1 and C2: 47 nF C0G across A0–A1 and A1–A2 | [option-b/BOM.md](option-b/BOM.md) |

The lists are generated from `scripts/boards/circuit.py`, so they always match the boards. Part numbers are given only where they were checked against the maker's data and Digi-Key UK. Everything else is a specification, and any part that meets it will do. Prices and order quantities are deliberately not guessed.

**The display plug is its own 8-way 2.54 mm header** (Samtec TSW-108-07-G-S, or any 8-way 2.54 mm header). The header strip that comes with the module goes on the module: 6 pins for the no-board build, all 12 for a board.

**What's no longer needed** compared with the earlier ADC's circuit:
- the 470 Ω pull-up;
- the 1 kΩ / 100 nF input filters (option B's two capacitors replace them, and go only across pin pairs, never to ground);
- the 100 kΩ ground-sense bias (the ADC's burn-out source does that job);
- the clamp diodes and TVS.

**Protection:** the series resistors keep a harness wire shorted to battery voltage within the ADC's input-current rating, but they are **not** load-dump or surge qualification. Bench test the short to battery (WIRING.md step 12), then check the installed system in the car.

**Avoid mounting a breadboard in a vehicle.** The car build uses soldered joints, strain relief and an insulated box. Check every connected ground potential before applying power, and never power the display from USB and another supply at the same time.
