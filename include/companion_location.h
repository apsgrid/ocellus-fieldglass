#pragma once
#include <Arduino.h>

struct CompanionLocationSnapshot {
  bool connected = false;
  bool authenticated = false;
  bool valid = false;
  bool mock = false;
  bool hasSpeed = false;
  bool hasBearing = false;
  uint32_t receivedMs = 0;
  uint32_t unixTimeS = 0;
  double lat = 0.0;
  double lon = 0.0;
  float accuracyM = NAN;
  float speedMps = NAN;
  float bearingDeg = NAN;
};

static constexpr const char* FIELDGLASS_LOCATION_SERVICE_UUID =
  "7b6f0001-8f60-4d55-a8b4-4649454c4447";
static constexpr const char* FIELDGLASS_LOCATION_FIX_UUID =
  "7b6f0002-8f60-4d55-a8b4-4649454c4447";

void companionLocationInit(const String& deviceName, uint32_t pairingPin);
bool companionLocationSnapshot(CompanionLocationSnapshot& out);
bool companionLocationTakeDirty(CompanionLocationSnapshot& out);
bool companionLocationFresh(const CompanionLocationSnapshot& fix, uint32_t maxAgeMs = 10000);
uint32_t companionLocationPairingPin();