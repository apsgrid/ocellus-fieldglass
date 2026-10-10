# Fieldglass BLE Location Companion Protocol v1

The companion protocol lets a phone or other BLE-capable device provide location to Ocellus Fieldglass without making Fieldglass dependent on a particular phone model, account, cloud service or APS.

## Privacy model

Coordinates are **not broadcast in BLE advertisements**.

Fieldglass advertises only:
- a generic device name such as `Fieldglass-9110`
- the 128-bit Fieldglass Location service UUID

The companion connects over BLE GATT. The location characteristic requires an encrypted, MITM-authenticated connection and the ESP32 requests bonding with a six-digit passkey shown on the Fieldglass screen.

## UUIDs

Service:

`7b6f0001-8f60-4d55-a8b4-4649454c4447`

Primary fix characteristic:

`7b6f0002-8f60-4d55-a8b4-4649454c4447`

The characteristic supports WRITE and WRITE WITHOUT RESPONSE.

## Primary fix packet

Exactly 20 bytes, little-endian, so it fits the minimum BLE ATT payload without requiring MTU negotiation.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | protocol/type = `0x11` (v1 primary fix) |
| 1 | 1 | flags |
| 2 | 4 | Unix UTC seconds, unsigned |
| 6 | 4 | latitude × 1e7, signed |
| 10 | 4 | longitude × 1e7, signed |
| 14 | 2 | horizontal accuracy in decimetres |
| 16 | 2 | speed in centimetres/second |
| 18 | 2 | bearing in centidegrees |

Flags:
- bit 0: valid fix
- bit 1: speed valid
- bit 2: bearing valid
- bit 3: Android reports a mock location
- remaining bits reserved

## Fieldglass behaviour

A companion fix is considered current for 10 seconds.

While current:
- radio observations receive `location.source = companion`
- UTC is estimated from the latest companion fix and Fieldglass monotonic time
- time quality is `companion`
- speed and heading are attached when valid

A standalone `location.fix` event is journalled:
- about once per second on SD
- about once every five seconds on internal flash

This reduces onboard flash wear while retaining enough points for a walking route.

If the link disappears or a fix ages beyond 10 seconds, Fieldglass immediately stops attaching the stale location. It does not invent coordinates.

## Android reference client

The open-source Android client is under `companion/android`.

It uses:
- Android BLE GATT APIs
- Android `LocationManager`
- a location foreground service while sharing

It deliberately has no Google Play Services dependency.

Any other platform may implement the same GATT protocol.