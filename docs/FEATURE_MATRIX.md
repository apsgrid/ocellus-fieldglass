# Capability matrix

## E32R40T / classic ESP32: planned first-class

| Capability | Status | Notes |
|---|---|---|
| 2.4 GHz Wi-Fi AP survey | native | SSID/BSSID/channel/RSSI/security |
| Promiscuous 802.11 metadata | native | management/control/data metadata with filtering |
| Wi-Fi management IE parsing | native | RSN/WPS/vendor/HT/capability fingerprints |
| Channel metrics | derived | packet density and observed airtime; not a spectrum analyser |
| Wi-Fi CSI | experimental | opt-in artifact capture; high volume |
| BLE passive scan | native | advertisements, UUIDs, manufacturer/service data metadata |
| Bluetooth Classic inquiry | native | name, CoD, EIR, RSSI |
| GNSS | add-on | UART GNSS module |
| microSD journal | native | independent SPI bus |
| Battery voltage | native | GPIO34, divider x2; calibrate per board |
| Touch UI | native | ST7796 + XPT2046 |
| Audio/alerts | native | DAC/speaker path |
| RGB status LED | native | three GPIO channels |
| WiGLE CSV export | host/export | compatibility exporter |
| GeoJSON / GPX export | host/export | route and observation export |
| JSONL export | host/export | lossless public interchange |
| Kismet-friendly import/export | host/export | preserve datasource/time/location/radio metadata |
| Short PCAPNG diagnostics | optional | authorised networks only; not default |
| 5 GHz / 6 GHz Wi-Fi | unsupported | requires different radio hardware |
| BLE 5 extended/periodic advertising | unsupported | classic ESP32 limitation |
| Wi-Fi FTM/RTT | not assumed | reserve schema capability, do not claim on this board |

## Profiles

DRIVE: fast channel rotation, GNSS prioritised, reduced screen work.
WALK: balanced Wi-Fi/BLE/Classic discovery, responsive UI.
SITE: slower dwell, richer frame metadata and channel metrics.
BLE: BLE-heavy schedule with intermittent Wi-Fi.
CLASSIC: Bluetooth Classic inquiry-heavy schedule.
CSI LAB: fixed/selected channel, explicit opt-in CSI recording.
BATTERY: conservative scan duty cycle and dim display.