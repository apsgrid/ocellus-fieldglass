# Fieldglass Companion for Android

This app turns a compatible Android phone into a live location/time source for Ocellus Fieldglass.

It is not tied to a Pixel, account, server or APS installation.

## Use

1. Power on Fieldglass.
2. On Fieldglass, note the six-digit code shown on the **Location** row.
3. Install and open **Fieldglass Companion**.
4. Grant Bluetooth and precise-location permissions.
5. Tap **Scan for Fieldglass**.
6. Select the Fieldglass you want to use.
7. Tap **Start sharing location**.
8. The first time, Android will request Bluetooth pairing. Enter the six-digit code shown by Fieldglass.
9. When the Fieldglass Location row turns green and shows an accuracy value, observations are being geotagged.

The app runs a visible foreground location service while sharing. Stop sharing from the app when you are finished.

## Privacy

The phone does not broadcast its coordinates.

Fieldglass advertises only a generic BLE service. Exact position data is written over the bonded/encrypted BLE GATT connection.

The app has no analytics, accounts or Google Play Services dependency.

Location is stored by Fieldglass according to the Fieldglass session/privacy settings. A default raw session stores precise coordinates.

## Compatibility

- Android 8.0 / API 26 or newer is the intended minimum.
- BLE is required.
- GPS/GNSS is preferred. Network location is used only when GNSS is unavailable.
- Other operating systems can implement the open protocol in ../../docs/COMPANION_PROTOCOL.md.

## Build

From this directory with Android SDK 35 and Java 17 available:

    gradle :app:assembleDebug

The repository GitHub Action also builds a debug APK automatically.

## Protocol

The app sends a fixed 20-byte little-endian packet once Android provides a location update. The packet is deliberately no larger than the minimum BLE characteristic payload.

See ../../docs/COMPANION_PROTOCOL.md.