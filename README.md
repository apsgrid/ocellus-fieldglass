# Ocellus Fieldglass

**Open, offline-first wireless field survey instrument for the E32R40T 4-inch ESP32 CYD, with Wi-Fi, BLE, phone/GNSS geolocation and extensible radio sensing.**

Ocellus Fieldglass turns the 4-inch E32R40T CYD into a portable wireless survey instrument. It is designed for passive field observation, authorised diagnostics and repeatable data collection rather than offensive Wi-Fi tooling.

> **Status:** active development. The current firmware is usable in the field, but the project has not reached a 1.0 hardware-support guarantee.

## Current target

Fieldglass currently targets the **E32R40T 4-inch classic ESP32 CYD**:

- ESP32-D0WD-V3 / ESP32-WROOM-32E
- 4 MB flash, no PSRAM
- ST7796 320x480 TFT
- XPT2046 resistive touch
- microSD
- battery-voltage ADC
- RGB LED
- DAC / speaker path

Other CYD variants are **not assumed compatible**. Display controllers, touch controllers, pinouts and MCU families vary significantly between boards sold under the CYD name.

See [Hardware compatibility](docs/HARDWARE.md).

## What Fieldglass does today

### Wireless survey

- passive 2.4 GHz Wi-Fi AP discovery
- SSID, BSSID, RSSI, channel and security metadata
- passive BLE advertisement discovery
- BLE address/name/RSSI plus basic advertisement metadata
- bounded radio caches to protect RAM on the classic ESP32
- staggered Wi-Fi/BLE scheduling for stability

### Location

Fieldglass can use a phone as its location/time source through the open **Fieldglass BLE Location Companion Protocol**.

- generic Android support — not tied to a Pixel or a specific account
- encrypted, bonded BLE GATT connection
- six-digit pairing PIN displayed by Fieldglass
- coordinates are **not broadcast** in BLE advertisements
- UTC, latitude/longitude, accuracy, speed and bearing
- stale fixes are rejected rather than silently reused

The reference Android client is in [companion/android](companion/android/).

See [Companion protocol](docs/COMPANION_PROTOCOL.md).

### Field logging

- JSONL observation journal
- unique session/event identifiers
- internal LittleFS fallback when no SD card is fitted
- SD-preferred logging when available
- protected flash reserve
- batched filesystem commits
- low-voltage write protection
- reset-cause telemetry
- panic/watchdog boot-loop SAFE MODE
- USB serial log inspection/export

The public observation model intentionally describes **radio observations, not people**. See [Ontology](docs/ONTOLOGY.md).

## Quick start

### Build the firmware

Requirements:

- PlatformIO
- USB serial connection to the E32R40T

Clone the repository and run:

    pio run

Flash the connected board:

    pio run -t upload --upload-port <PORT>

On first boot, follow the touch-calibration markers if calibration has not already been stored.

For a more complete walkthrough, see [Quick start](docs/QUICKSTART.md).

### Android location companion

The companion app uses standard Android BLE and LocationManager APIs and deliberately has no Google Play Services dependency.

From companion/android:

    gradle :app:assembleDebug

GitHub Actions also builds a debug APK artifact on relevant commits.

Usage:

1. Power on Fieldglass.
2. Open **Fieldglass Companion** on the Android phone.
3. Grant Bluetooth and precise-location permissions.
4. Scan for Fieldglass.
5. Select the desired unit.
6. Tap **Start sharing location**.
7. Enter/confirm the six-digit PIN shown on Fieldglass.
8. A green Location row with an accuracy value means fixes are being attached to observations.

## Serial maintenance

At 115200 baud:

    STATUS
    FILES
    DUMP <path>
    STOP
    START
    SAFE CLEAR
    HELP

Destructive flash repair requires:

    FORMAT FLASH CONFIRM

Fieldglass deliberately does **not** auto-format a failed journal filesystem because preserving field data is more important than automatic recovery.

The host helper is:

    python tools/fieldglass_cli.py --port <PORT> status

## Repository layout

    src/                  ESP32 firmware
    include/              board and public firmware headers
    schemas/              observation JSON Schema
    docs/                 architecture, protocol and hardware docs
    companion/android/    Android reference location companion
    tools/                 host maintenance utilities
    sim/                   synthetic event generator
    tests/                 host-side protocol/schema tests
    .github/workflows/     firmware + Android CI builds

## Stability philosophy

The current WALK profile favours:

- bounded memory over unbounded discovery lists
- passive observation over unnecessary RF transmission
- explicit data-preservation behaviour over automatic formatting
- safe degradation over reboot loops
- durable event records over maximum packet capture volume

See [Stability and recovery](docs/STABILITY.md).

## Privacy and scope

Fieldglass is intended for surveying your own networks/devices, authorised wireless diagnostics, coverage/channel assessment, passive environmental observation, and research/development.

It does not ship deauthentication, credential capture, password cracking, evil-twin or exploit modules. Raw application payloads are not retained by default.

Field captures and recovered radio data are intentionally excluded from the repository by .gitignore.

## Roadmap

### 0.3.x — Field instrument

- richer Wi-Fi management metadata and fingerprinting
- richer BLE decoding
- channel activity metrics
- device detail/history UI
- session browser
- hardware GNSS input

### 0.4 — Radio lab

- explicit Bluetooth Classic profile
- opt-in Wi-Fi CSI mode
- advanced channel metrics

### 0.5 — Geo/export

- direct UART GNSS
- GPX
- GeoJSON
- WiGLE-compatible export

### 1.0

- stable public hardware profile
- documented flashing/recovery path
- release binaries
- tested companion workflow
- published compatibility matrix

## Contributing

Bug reports, hardware observations and pull requests are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

No open-source licence has been selected yet. Until a licence file is added, normal copyright rules apply.