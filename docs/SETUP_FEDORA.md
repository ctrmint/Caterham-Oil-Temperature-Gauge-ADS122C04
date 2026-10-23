# Building and flashing on Fedora

The project uses **native ESP-IDF 5.5.5**, **LVGL 9.4.x**, and Waveshare's managed `esp32_s3_touch_amoled_1_75` **3.x BSP**. This avoids pin assumptions and incompatible display-driver forks. External components are downloaded during your **first** `idf.py build`; this ZIP does **not** contain those third-party libraries.

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
cd ~/Downloads/caterham-oil-temp-gauge-waveshare # or where you extracted the ZIP
. "$HOME/esp/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

Do not ignore compilation warnings/errors. Compare component-manager resolved versions against the manifest and Waveshare's example if the BSP changes. For immutable dependency builds, commit the generated `dependencies.lock` after a successful hardware build.

## Connect the board and flash

1. Connect the ESP32-S3 to Fedora by a USB **data** cable (use a clean bench supply, **not car 12 V**).
2. Discover the port with `ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null` and `sudo dmesg | tail`.
3. Confirm USB serial permissions. Fedora `uucp` or `dialout` group may be needed depending on udev configuration; log out/in after adding a group.
4. Run:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Press `Ctrl+]` to exit the IDF serial monitor. Firmware replaces the vendor demo; consult vendor recovery images for restoration.

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

These tests require no ESP-IDF. At project creation the host tests were run successfully, but **full board compilation and live display bring-up remain unverified** until a physical board is available.

## Common faults

| Symptom | Check |
|---|---|
| Display black | Correct *1.75* variant, 5 V power, Waveshare BSP 3.x, PSRAM config |
| Chip not detected | Data USB cable, correct port, BOOT/recovery instructions |
| ADC unavailable | GPIO16/17 wiring, ADC2 Wi-Fi disabled, eFuse calibration, broken ADC drivers |
| Supply fault | H2 ground, reference divider 10k/10k, GPIO17 voltage ≈1.65 V |
| Sensor open | Sender spade / tank earth missing, actual high resistance outside design range |
| Temperature stays `CALIBRATION REQUIRED` | Deliberate safe default; measure the sender and generate calibrated header |
| Gauge shows unrealistic temp | Wrong calibration table, thermal coupling, ADC saturation, ground offsets, wrong board firmware |
