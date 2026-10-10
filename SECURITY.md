# Security policy

Ocellus Fieldglass is an experimental wireless field instrument.

## Reporting a vulnerability

Please do not publish a working exploit as the first disclosure. Use GitHub private vulnerability reporting/security advisories when available; otherwise open a minimal issue requesting a private contact channel without exploit details.

Useful reports include BLE pairing/authentication bypasses, unintended coordinate disclosure, remote crashes, malformed-radio-input crashes, journal corruption paths and unsafe update behaviour.

## Security principles

- coordinates are not broadcast in BLE advertisements
- companion location requires a bonded/encrypted BLE link
- raw field data is not committed to the public repository
- storage failure should fail safe rather than erase evidence automatically
- stale location must not be silently reused