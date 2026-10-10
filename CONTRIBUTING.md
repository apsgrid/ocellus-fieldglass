# Contributing to Ocellus Fieldglass

Thanks for helping improve Fieldglass.

## Before opening a pull request

Run the relevant tests:

    pio run
    python sim/generate_sample.py
    python tests/test_sample.py
    python tests/test_companion_protocol.py

## Hardware changes

If adding another CYD or ESP32 board, add a distinct board profile. Document exact board identifier, MCU, display/touch controller, resolution, flash/PSRAM, SD wiring, battery sensing and tested features.

## Radio changes

Fieldglass is intentionally a passive/diagnostic survey instrument. Contributions should not add deauthentication, credential attacks, password cracking, evil-twin workflows, exploit payloads or arbitrary application-payload interception.

Passive metadata, authorised diagnostics, channel analysis and instrumentation are in scope.

## Stability requirements

Changes touching BLE/Wi-Fi scanning, filesystems, battery handling or task scheduling deserve soak testing. Include minimum heap, reset behaviour and test duration for substantial changes.

## Data and privacy

Do not commit real field captures, home/private coordinates, Wi-Fi passwords, API credentials, private MAC inventories or recovered flash images. Use synthetic fixtures.

## Coding style

Prefer straightforward, bounded code suitable for a no-PSRAM classic ESP32. Avoid unbounded containers, heavy callback work, callback filesystem writes, unnecessary heap churn and silent destructive recovery.