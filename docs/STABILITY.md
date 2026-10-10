# Stability and recovery contract

Fieldglass treats data preservation as a higher priority than automatic recovery.

## Reset telemetry

Every boot records:

- firmware version
- persistent boot counter
- ESP reset reason
- battery voltage
- free heap
- minimum free heap

Reset reasons distinguish power-on/external reset, software reset, panic, watchdog, brownout and deep-sleep cases where the ESP32 exposes that distinction.

## Internal-flash protection

LittleFS is mounted with format-on-fail disabled.

If mounting fails:
- Fieldglass enters STORAGE ERROR.
- Radio/UI operation may continue, but recording is locked.
- Existing bytes are not erased.

If free space approaches the protected reserve:
- recording is stopped,
- the journal is flushed/closed,
- STORAGE LOCKED is displayed.

Formatting is explicit through the serial command FORMAT FLASH CONFIRM.

## Write behaviour

Observations are appended as newline-delimited JSON.

Writes are flushed when:
- 8 new lines have accumulated,
- 3 seconds have elapsed since the previous flush,
- the session is stopped,
- a power-protection transition occurs.

A sudden loss can therefore lose only the unflushed tail; complete prior lines remain individually parseable.

## Battery protection

The voltage estimate is sampled repeatedly.

- low-voltage mode reduces scan duty cycle
- three consecutive critical samples stop the session and close storage
- recovery requires three healthy samples above the recovery threshold
- recording does not automatically restart after voltage recovery

The voltage-to-percentage curve is an estimate and should be calibrated against the actual battery/charger path.

## Radio-memory protection

The current WALK profile:
- releases Bluetooth Classic controller memory
- uses passive BLE scanning
- uses a fixed-size BLE cycle table
- caps per-cycle BLE entries
- clears scanner results after each cycle
- staggers Wi-Fi and BLE scans

The firmware reports dropped BLE entries and the minimum observed heap to serial.

## Known design choice

The current scanners are synchronous. This intentionally favours predictable memory ownership and stability over maximum UI responsiveness. Asynchronous scanning should only replace them once equivalent soak tests exist.

## Boot-loop breaker

Panic and watchdog resets are tracked in NVS. After three consecutive fault resets, Fieldglass boots into SAFE MODE.

SAFE MODE:
- leaves the radio survey engines disabled
- does not automatically start a recording session
- keeps the UI, battery monitor and serial maintenance channel available
- attempts non-destructive storage mounting so existing journals can be listed/exported
- can be explicitly cleared with the serial command SAFE CLEAR

A normal non-fault reset clears the consecutive-fault count.