# Hardware compatibility

## Validated board

The initial Fieldglass hardware profile is the **E32R40T 4-inch classic ESP32 CYD**.

| Component | Target |
|---|---|
| MCU | ESP32-D0WD-V3 / ESP32-WROOM-32E |
| Flash | 4 MB |
| PSRAM | none |
| Display | ST7796 |
| Resolution | 320x480 |
| Touch | XPT2046 resistive |
| SD | SPI microSD |
| Battery ADC | GPIO34 through board divider |

The exact pin map lives in `include/board_e32r40t.h` and the TFT build flags in `platformio.ini`.

## Why the exact CYD matters

CYD is a community nickname, not a single hardware standard. Boards may differ in MCU, display controller, touch controller, resolution, SD wiring, PSRAM, USB bridge, battery circuit and exposed GPIOs.

Do not assume an ESP32-S3 CYD or another 4-inch panel is pin-compatible with this target.

## Current constraints

The classic ESP32 target provides 2.4 GHz Wi-Fi, BLE and Bluetooth Classic capability. It does not provide 5 GHz Wi-Fi or the BLE 5 extended/periodic advertising feature set.

The current WALK firmware releases Bluetooth Classic controller memory and uses BLE only. Bluetooth Classic will return as an explicit profile.

## Optional hardware

### microSD

Recommended for long sessions and future CSI/high-volume capture modes. Short runs can use onboard LittleFS.

### Hardware GNSS

UART GNSS is planned as a first-class location source. The location model is already source-agnostic so phone companion fixes and hardware GNSS can coexist.

### Speaker

The board audio path can support future alerts/search feedback.