# Building and flashing on Fedora

The project uses **native ESP-IDF 5.5.5**, **LVGL 9.4.x**, and Waveshare's managed `esp32_s3_touch_amoled_1_75` **3.x BSP**. This avoids pin assumptions and incompatible display-driver forks. External components are downloaded during your **first** `idf.py build`; the repository does **not** contain those third-party libraries.

## Install IDF

```bash
sudo dnf install -y git cmake ninja-build ccache dfu-util libusbx-devel flex bison gperf \
  python3 python3-pip python3-virtualenv openssl-devel wget
mkdir -p ~/esp && cd ~/esp
git clone --branch v5.5.5 --depth 1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. "$HOME/esp/esp-idf/export.sh"
```

If distribution package names differ, use Espressif's current Fedora/Linux setup instructions. Do not install the build dependencies globally with `sudo pip`.

## Build the project

```bash
cd Caterham-Oil-Temperature-Gauge   # your clone of this repository
. "$HOME/esp/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

Do not ignore compilation warnings/errors. The component versions are pinned in the committed `dependencies.lock`, so a build should leave it unchanged. If the BSP is ever updated, compare the resolved versions against the manifest and Waveshare's example, and commit the new `dependencies.lock` only after a successful hardware test.

## Connect the board and flash

1. Connect the ESP32-S3 to Fedora by a USB **data** cable (use a clean bench supply, **not car 12 V**).
2. Check that the board is the gauge: `lsusb | grep 303a:1001` should print `Espressif USB JTAG/serial debug unit`.
3. Find its port with `ls -l /dev/serial/by-id/usb-Espressif_*`. Always flash and monitor through this `by-id` path, never a bare `/dev/ttyACM0`. ACM numbers are handed out in plug-in order, so `ttyACM0` can be a different USB device (on the development laptop it has been a Raspberry Pi Pico). If more than one Espressif device is listed, unplug the others.
4. Confirm USB serial permissions. Fedora `uucp` or `dialout` group may be needed depending on udev configuration; log out/in after adding a group.
5. Run:

```bash
PORT=$(ls /dev/serial/by-id/usb-Espressif_*)
idf.py -p "$PORT" flash monitor
```

If the flash cannot connect, hold **BOOT**, tap **RESET**, release BOOT and run it again. Press `Ctrl+]` to exit the IDF serial monitor. Firmware replaces the vendor demo; consult vendor recovery images for restoration.

## Synthetic demo (bench only)

```bash
idf.py menuconfig
```

Enable **Caterham oil temperature gauge → Display synthetic sensor readings (BENCH DEMO ONLY)**; then rebuild/flash. The display visibly reads `DEMO ONLY - NOT REAL` and sensor data is synthetic. **Disable the option** and reflash before connecting to the car. For physical sensor operation, the board shows ohms until you provide validated calibration.

## Host-only tests

```bash
sudo dnf install -y gcc-c++ python3
./scripts/test_host.sh
```

These tests require no ESP-IDF. The host tests pass and the firmware compiles cleanly with ESP-IDF 5.5.5. Flashing and display bring-up have been verified on a 31261 board (see [VALIDATION.md](VALIDATION.md)). Every bench test of the ADS122C04 front end is still owed.

## Common faults

| Symptom | Check |
|---|---|
| Display black | Correct *1.75* variant, USB-C power, Waveshare BSP 3.x, PSRAM config |
| Chip not detected | Data USB cable, correct port, BOOT/recovery instructions |
| ADC unavailable | ADS122C04 not found at 0x40: SDA/SCL swapped (GPIO17 = SDA, GPIO18 = SCL), an address jumper closed, missing 3.3 V/GND to the module, display lead too long. A persistent "read failed" with the module found points to a CRC, register or counter mismatch: check the lead and its grounds |
| Supply fault | 3.3 V on VIN out of range; or the excitation current `ref_v` / Rref is outside 210–290 µA: Rref missing, the wrong value, or not on A0, or `kReferenceOhms` not set to the measured value |
| Sensor open | Sender spade disconnected, broken signal wire, R1's joint open, or no return path to the tank (return wire off with the display on a power bank). Also a genuinely high resistance: over 5 kΩ is a sender colder than about 4 °C on the Bosch curve |
| Sensor short | Signal wire shorted to the tank or chassis; Rref's joint at A0 open |
| Ground fault | Ground-sense wire disconnected or R2's joint open (the ADC's burn-out source pulls it up), or landed on chassis instead of the tank stud; poor tank bond; tank more than 0.1 V below or 0.2 V above gauge GND. The CSV log's last column shows the measured offset |
| Temperature stays `CALIBRATION REQUIRED` | Deliberate safe default; measure the sender and generate calibrated header |
| Gauge shows unrealistic temp | Wrong calibration table, thermal coupling, ADC saturation, ground offsets, wrong board firmware |
