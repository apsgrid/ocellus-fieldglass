# Fieldglass ontology v1

## Core classes

### Observer
The instrument or datasource that produced an observation. An observer has a stable device id, hardware profile, firmware version and capability set.

### Session
A bounded field run. A session owns start/end times, profile, privacy policy, optional route summary and monotonically increasing event sequence.

### Observation
An immutable statement that Observer X observed Subject Y at time T, optionally at location L, using modality M.

### Subject
The thing directly visible to a radio protocol. A subject is intentionally not assumed to be a person, vehicle or durable real-world entity.

Subject kinds include:
- wifi_bssid
- wifi_station
- wifi_ssid
- ble_advertiser
- bt_classic_device
- radio_channel

### Identifier
A protocol-level identifier or advertised attribute. Identifiers carry a type and a confidence/stability class because many modern devices randomise addresses.

### LocationFix
A geospatial sample with coordinates plus its quality: source, accuracy, fix type, satellites, HDOP, speed and heading.

### SignalSample
Radio metadata attached to a sighting: RSSI, channel/frequency, PHY mode, rate, MCS, antenna and optional noise/airtime fields.

### Fingerprint
A derived, non-payload signature computed from observable protocol metadata. Examples include Wi-Fi information-element sets, RSN capability sets and BLE advertisement structure signatures. Fingerprints never assert identity by themselves.

### DerivedMetric
A calculation over observations, such as per-channel frame density, observed airtime estimate, RSSI distribution, route statistics or first/last-seen counts.

### CaptureArtifact
An optional high-volume artifact referenced by an event instead of embedded in it. Examples: short authorised PCAP/PCAPNG diagnostic captures and CSI blocks.

### PrivacyTransform
Records what was altered before storage/export: none, redact, pseudonymise or geofence-fuzz.

## Relationships

observer --OBSERVED--> subject
observation --PART_OF--> session
observation --AT--> location_fix
observation --HAS_SIGNAL--> signal_sample
observation --HAS_IDENTIFIER--> identifier
observation --DERIVED_TO--> fingerprint
metric --DERIVED_FROM--> observation set
artifact --REFERENCED_BY--> observation/session

The public ontology deliberately does not contain PERSON, VEHICLE, CASE or OWNER. Those are integration-layer concepts.

## Identity discipline

A MAC address is an identifier, not an entity.

For BLE random/private addresses and locally administered Wi-Fi addresses, Fieldglass records address type and local-administered state. Correlation across changing identifiers may be represented only as a hypothesis with an explicit method and confidence outside the raw observation layer.

## Event kinds

- session.start
- session.end
- system.status
- gnss.fix
- location.fix
- wifi.ap
- wifi.station
- wifi.frame_summary
- wifi.channel_metric
- wifi.csi_ref
- ble.advertisement
- bt.discovery
- artifact.ref
- annotation

## Wi-Fi ontology

wifi.ap supports:
- BSSID and SSID
- channel/frequency
- RSSI
- privacy/security family
- beacon/probe-response distinction
- capability bits
- RSN/WPA/WPS metadata
- 802.11n HT capabilities visible to this receiver
- PMF capability/requirement
- vendor information-element OUIs
- information-element fingerprint
- first/last seen and observation count are derived, not stored as raw truth

wifi.frame_summary supports packet metadata without retaining payload:
- management/control/data class
- subtype
- PHY/rate/sig_mode
- packet length
- aggregation indicator when available
- CRC/error state when exposed
- transmitter/receiver/BSSID addresses when present

wifi.csi_ref references a separate CSI artifact. CSI is opt-in because it is high volume.

## BLE ontology

ble.advertisement supports:
- advertiser address and address type
- RSSI
- advertisement type
- local name
- flags
- appearance
- TX power
- manufacturer company id
- manufacturer-data digest
- service UUIDs
- service-data digest
- raw advertisement retention flag (default false)
- advertisement-structure fingerprint

## Bluetooth Classic ontology

bt.discovery supports:
- BD_ADDR
- RSSI
- device name
- Class of Device
- major service class
- major/minor device class
- Extended Inquiry Response fields/fingerprint

## Location ontology

A location fix contains both values and quality. Missing GNSS never creates fake coordinates.

Fields:
- lat, lon, alt_m
- accuracy_m
- speed_mps
- heading_deg
- satellites
- hdop
- fix_type
- source: gnss | companion | host | manual | none
- timestamp_utc

## Time ontology

Every event always has monotonic_ms and sequence. UTC is nullable until a trusted clock exists.
time_quality is one of:
- gnss
- companion
- network
- rtc
- estimated
- unknown

## Privacy ontology

privacy.mode:
- raw
- pseudonymised
- redacted

privacy.location:
- precise
- fuzzed
- suppressed

privacy identifier hashing uses a per-install or per-session secret. Export tools must include the transformation metadata so consumers know the identifiers are not raw.

## Integration boundary

An integration may add a separate correlation layer:
- integration entity reference
- hypothesis relationship
- confidence
- analyst annotation
- server-side geospatial indexing

It must not mutate the immutable raw observation.