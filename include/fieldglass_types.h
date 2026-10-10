#pragma once
#include <Arduino.h>

namespace fg {

static constexpr uint16_t SCHEMA_MAJOR = 1;
static constexpr uint16_t SCHEMA_MINOR = 0;

enum class EventKind : uint8_t {
  SessionStart,
  SessionEnd,
  SystemStatus,
  GnssFix,
  LocationFix,
  WifiAp,
  WifiStation,
  WifiFrameSummary,
  WifiChannelMetric,
  WifiCsiRef,
  BleAdvertisement,
  BtDiscovery,
  ArtifactRef,
  Annotation
};

enum class RadioKind : uint8_t { None, Wifi24, Ble, BtClassic };
enum class TimeQuality : uint8_t { Unknown, Estimated, Rtc, Network, Companion, Gnss };
enum class LocationSource : uint8_t { None, Gnss, Companion, Host, Manual };
enum class PrivacyMode : uint8_t { Raw, Pseudonymised, Redacted };
enum class LocationPrivacy : uint8_t { Precise, Fuzzed, Suppressed };

struct TimePoint {
  uint64_t utc_ms = 0;          // 0 when unknown
  uint32_t monotonic_ms = 0;
  TimeQuality quality = TimeQuality::Unknown;
};

struct LocationFix {
  bool valid = false;
  double lat = 0;
  double lon = 0;
  float alt_m = NAN;
  float accuracy_m = NAN;
  float speed_mps = NAN;
  float heading_deg = NAN;
  float hdop = NAN;
  uint8_t satellites = 0;
  uint8_t fix_type = 0;
  LocationSource source = LocationSource::None;
};

struct SignalSample {
  int8_t rssi_dbm = -127;
  int8_t noise_dbm = 127;       // 127 = unavailable
  uint16_t frequency_mhz = 0;
  uint8_t channel = 0;
  uint8_t phy_mode = 0;
  uint8_t rate_code = 0;
  uint8_t mcs = 0xFF;
  uint8_t antenna = 0xFF;
  uint16_t frame_len = 0;
};

struct PrivacyState {
  PrivacyMode mode = PrivacyMode::Raw;
  LocationPrivacy location = LocationPrivacy::Precise;
  bool ssid_redacted = false;
};

struct EventEnvelope {
  uint64_t session_id = 0;
  uint32_t sequence = 0;
  EventKind kind = EventKind::SystemStatus;
  RadioKind radio = RadioKind::None;
  TimePoint time;
  LocationFix location;
  SignalSample signal;
  PrivacyState privacy;
};

struct WifiApPayload {
  uint8_t bssid[6]{};
  char ssid[33]{};
  uint8_t auth_mode = 0;
  bool hidden = false;
  bool locally_administered = false;
  bool pmf_capable = false;
  bool pmf_required = false;
  uint32_t ie_fingerprint = 0;
};

struct BlePayload {
  uint8_t address[6]{};
  uint8_t address_type = 0xFF;
  char name[32]{};
  int8_t tx_power = 127;
  uint16_t appearance = 0;
  uint16_t manufacturer_id = 0xFFFF;
  uint32_t adv_fingerprint = 0;
};

struct BtClassicPayload {
  uint8_t address[6]{};
  char name[64]{};
  uint32_t class_of_device = 0;
  uint32_t eir_fingerprint = 0;
};

} // namespace fg