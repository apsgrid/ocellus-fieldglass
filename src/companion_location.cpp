#include "companion_location.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEAdvertising.h>
#include <BLESecurity.h>
#include <esp_gatt_defs.h>

namespace {
portMUX_TYPE fixMux = portMUX_INITIALIZER_UNLOCKED;
CompanionLocationSnapshot currentFix;
volatile bool fixDirty = false;
volatile bool clientConnected = false;
volatile bool clientAuthenticated = false;
uint32_t currentPin = 0;

BLEServer* server = nullptr;
BLECharacteristic* fixCharacteristic = nullptr;
BLESecurity* security = nullptr;

uint16_t readU16LE(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint32_t readU32LE(const uint8_t* p) {
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

int32_t readI32LE(const uint8_t* p) {
  return (int32_t)readU32LE(p);
}

class LocationCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic, esp_ble_gatts_cb_param_t*) override {
    std::string value = characteristic->getValue();
    if (value.size() != 20) return;

    const uint8_t* p = reinterpret_cast<const uint8_t*>(value.data());
    if (p[0] != 0x11) return;  // protocol v1, primary fix packet

    uint8_t flags = p[1];
    CompanionLocationSnapshot next;
    next.connected = clientConnected;
    next.authenticated = clientAuthenticated;
    next.valid = (flags & 0x01) != 0;
    next.hasSpeed = (flags & 0x02) != 0;
    next.hasBearing = (flags & 0x04) != 0;
    next.mock = (flags & 0x08) != 0;
    next.unixTimeS = readU32LE(p + 2);
    next.lat = (double)readI32LE(p + 6) / 10000000.0;
    next.lon = (double)readI32LE(p + 10) / 10000000.0;
    next.accuracyM = (float)readU16LE(p + 14) / 10.0f;
    next.speedMps = next.hasSpeed ? (float)readU16LE(p + 16) / 100.0f : NAN;
    next.bearingDeg = next.hasBearing ? (float)readU16LE(p + 18) / 100.0f : NAN;
    next.receivedMs = millis();

    if (next.lat < -90.0 || next.lat > 90.0 ||
        next.lon < -180.0 || next.lon > 180.0) {
      return;
    }

    portENTER_CRITICAL(&fixMux);
    currentFix = next;
    fixDirty = true;
    portEXIT_CRITICAL(&fixMux);
  }
};

class LocationServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    clientConnected = true;
    portENTER_CRITICAL(&fixMux);
    currentFix.connected = true;
    portEXIT_CRITICAL(&fixMux);
  }

  void onDisconnect(BLEServer*) override {
    clientConnected = false;
    clientAuthenticated = false;
    portENTER_CRITICAL(&fixMux);
    currentFix.connected = false;
    currentFix.authenticated = false;
    currentFix.valid = false;
    portEXIT_CRITICAL(&fixMux);
    BLEDevice::startAdvertising();
  }
};

class LocationSecurityCallbacks : public BLESecurityCallbacks {
 public:
  uint32_t onPassKeyRequest() override { return currentPin; }
  void onPassKeyNotify(uint32_t) override {}
  bool onSecurityRequest() override { return true; }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    clientAuthenticated = cmpl.success;
    portENTER_CRITICAL(&fixMux);
    currentFix.authenticated = cmpl.success;
    portEXIT_CRITICAL(&fixMux);
  }

  bool onConfirmPIN(uint32_t pin) override {
    return pin == currentPin;
  }
};

LocationCharacteristicCallbacks characteristicCallbacks;
LocationServerCallbacks serverCallbacks;
LocationSecurityCallbacks securityCallbacks;
}  // namespace

void companionLocationInit(const String& deviceName, uint32_t pairingPin) {
  currentPin = pairingPin;

  // BLEDevice::init() is intentionally owned here so the public companion
  // protocol and the survey scanner share one controller instance.
  BLEDevice::init(deviceName.c_str());

  BLEDevice::setSecurityCallbacks(&securityCallbacks);
  security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  security->setCapability(ESP_IO_CAP_OUT);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setKeySize(16);
  security->setStaticPIN(currentPin);

  server = BLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);

  BLEService* service = server->createService(FIELDGLASS_LOCATION_SERVICE_UUID);
  fixCharacteristic = service->createCharacteristic(
    FIELDGLASS_LOCATION_FIX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  // Require an encrypted, MITM-authenticated link before accepting a position.
  fixCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENC_MITM);
  fixCharacteristic->setCallbacks(&characteristicCallbacks);
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(FIELDGLASS_LOCATION_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

bool companionLocationSnapshot(CompanionLocationSnapshot& out) {
  portENTER_CRITICAL(&fixMux);
  out = currentFix;
  portEXIT_CRITICAL(&fixMux);
  return out.valid;
}

bool companionLocationTakeDirty(CompanionLocationSnapshot& out) {
  bool dirty = false;
  portENTER_CRITICAL(&fixMux);
  if (fixDirty) {
    out = currentFix;
    fixDirty = false;
    dirty = true;
  }
  portEXIT_CRITICAL(&fixMux);
  return dirty;
}

bool companionLocationFresh(const CompanionLocationSnapshot& fix, uint32_t maxAgeMs) {
  return fix.valid && fix.connected && fix.authenticated &&
         (uint32_t)(millis() - fix.receivedMs) <= maxAgeMs;
}

uint32_t companionLocationPairingPin() {
  return currentPin;
}