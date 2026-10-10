# Quick start

This guide assumes an E32R40T 4-inch classic ESP32 CYD.

## 1. Check the board

Do not flash a random board sold as a CYD without checking the hardware.

The validated target uses:

- classic ESP32 / ESP32-WROOM-32E
- ST7796 display
- XPT2046 touch
- 320x480 panel

See [HARDWARE.md](HARDWARE.md).

## 2. Build

Install PlatformIO, then from the repository root:

    pio run

The expected target is `cyd4`.

## 3. Flash

Connect the CYD by USB and identify its serial port.

    pio run -t upload --upload-port <PORT>

## 4. First boot

Fieldglass should initialise the display, offer touch calibration if required, attempt SD mounting, fall back to onboard flash if no SD card is present, start the WALK profile, and show the Location pairing code.

No SD card is required for normal short field runs.

## 5. Optional phone location

Build/install the Android companion from `companion/android`. Grant Bluetooth and precise-location permissions, scan for Fieldglass, select the target, start sharing, and enter/confirm the PIN displayed by Fieldglass.

When the Location row turns green, radio observations receive a current phone-derived location.

## 6. Retrieve logs

List files over USB serial:

    FILES

Export a journal:

    DUMP /fieldglass-....jsonl

Or use:

    python tools/fieldglass_cli.py --port <PORT> files
    python tools/fieldglass_cli.py --port <PORT> dump <remote-path> output.jsonl

## 7. Before a long walk

- charge the battery
- check STATUS
- confirm free storage
- confirm phone location if using it
- use microSD for long/high-volume runs