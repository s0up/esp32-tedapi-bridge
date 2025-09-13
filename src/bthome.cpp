#include "bthome.h"
#include "powerwall.h"

// BTHome constants
// Service UUID for BTHome: 0xFCD2 (16-bit UUID in service data)
// Info byte (unencrypted, version 2): 0b01000000 = 0x40 (bit6=1 for v2, bit0=0 unencrypted)
// Battery object id in BTHome is 0x01 (uint8 percent)

static const uint16_t BTHOME_SERVICE_UUID_16 = 0xFCD2;
static const uint8_t BTHOME_INFO_UNENCRYPTED_V2 = 0x40;
static const uint8_t BTHOME_OBJ_MEASUREMENT_ID = 0x00; // u8 index for following measurements
static const uint8_t BTHOME_OBJ_BATTERY = 0x01; // uint8
static const uint8_t BTHOME_OBJ_POWER = 0x0B;   // s24 W (legacy)
static const uint8_t BTHOME_OBJ_POWER_32 = 0x5C; // s32, factor 0.01 W (preferred)
static const uint8_t BTHOME_OBJ_BOOLEAN = 0x0F;  // uint8 (0/1)

BTHomeAdvertiser::BTHomeAdvertiser()
  : lastBatteryPercent(255), started(false), advertising(nullptr), deviceName(""), frameFlip(false) {}

void BTHomeAdvertiser::begin(const String &deviceNameArg) {
  if (started) return;
  deviceName = deviceNameArg;
  NimBLEDevice::init(deviceName.c_str());
  // Increase TX power for visibility
  NimBLEDevice::setPower(ESP_PWR_LVL_P7);
  advertising = NimBLEDevice::getAdvertising();
  started = true;
  Serial.println("[BTHome] Advertiser initialized; waiting for first data update");
}

void BTHomeAdvertiser::updateBatteryPercent(uint8_t batteryPercent) {
  if (!started) return;
  if (batteryPercent > 100) batteryPercent = 100;
  if (lastBatteryPercent == batteryPercent && advertising && advertising->isAdvertising()) {
    return;
  }
  buildAdvertisement(batteryPercent);
  startAdvertising();
  lastBatteryPercent = batteryPercent;
}

static inline void append_u8(std::string &buf, uint8_t v) { buf.push_back((char)v); }
static inline void append_s24(std::string &buf, int32_t v) {
  if (v > 8388607) v = 8388607;
  if (v < -8388608) v = -8388608;
  uint32_t uv = (uint32_t)(v & 0x00FFFFFF);
  buf.push_back((char)(uv & 0xFF));
  buf.push_back((char)((uv >> 8) & 0xFF));
  buf.push_back((char)((uv >> 16) & 0xFF));
}
static inline void append_s32(std::string &buf, int32_t v) {
  buf.push_back((char)(v & 0xFF));
  buf.push_back((char)((v >> 8) & 0xFF));
  buf.push_back((char)((v >> 16) & 0xFF));
  buf.push_back((char)((v >> 24) & 0xFF));
}

void BTHomeAdvertiser::updateBatteryAndPowers(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t batteryPowerW, int32_t sitePowerW, bool gridConnected) {
  if (!started) return;
  if (batteryPercent > 100) batteryPercent = 100;
  buildAdvertisement(batteryPercent, solarPowerW, loadPowerW, batteryPowerW, sitePowerW, gridConnected);
  startAdvertising();
  lastBatteryPercent = batteryPercent;
}

void BTHomeAdvertiser::buildAdvertisement(uint8_t batteryPercent) {
  // Build BTHome service data payload: [info][id][value]
  // info: 0x40 (v2, unencrypted)
  // id: 0x01 (battery)
  // value: uint8 percent
  std::string serviceData;
  serviceData.reserve(3);
  serviceData.push_back((char)BTHOME_INFO_UNENCRYPTED_V2);
  serviceData.push_back((char)BTHOME_OBJ_BATTERY);
  serviceData.push_back((char)batteryPercent);

  // Prepare advertisement
  NimBLEAdvertisementData advData;
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setServiceData(NimBLEUUID(BTHOME_SERVICE_UUID_16), serviceData);

  NimBLEAdvertisementData scanResp;
  if (deviceName.length()) {
    scanResp.setName(deviceName.c_str());
  }

  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanResp);
}

void BTHomeAdvertiser::startAdvertising() {
  if (!advertising) return;
  advertising->stop();
  // Make advertiser connectable undirected so generic OS scanners can see it
  advertising->setAdvertisementType(BLE_HCI_ADV_TYPE_ADV_IND);
  advertising->setMinInterval(0x00A0); // 100ms
  advertising->setMaxInterval(0x00F0); // 150ms
  advertising->setScanResponse(true);
  advertising->start();
  Serial.println("[BTHome] Advertising started (UUID 0xFCD2, unencrypted v2)");
}

void BTHomeAdvertiser::buildAdvertisement(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t batteryPowerW, int32_t sitePowerW, bool gridConnected) {
  // Alternate compact frames to stay within 31-byte ADV limit
  // Use Power_32 (0x5C) with factor 0.01 W for compatibility
  // Frame A: battery + solar(id1) + load(id2) + site(id3)
  // Frame B: battery + batteryPower(id4) + grid boolean
  std::string serviceData;
  serviceData.reserve(32);
  append_u8(serviceData, BTHOME_INFO_UNENCRYPTED_V2);

  // battery percent
  append_u8(serviceData, BTHOME_OBJ_BATTERY);
  append_u8(serviceData, batteryPercent);

  auto append_power_5c = [&](uint8_t measurementId, int32_t watts) {
    int64_t scaled = (int64_t)watts * 100LL; // factor 0.01 W
    if (scaled > INT32_MAX) scaled = INT32_MAX;
    if (scaled < INT32_MIN) scaled = INT32_MIN;
    append_u8(serviceData, BTHOME_OBJ_MEASUREMENT_ID);
    append_u8(serviceData, measurementId);
    append_u8(serviceData, BTHOME_OBJ_POWER_32);
    append_s32(serviceData, (int32_t)scaled);
  };

  if (!frameFlip) {
    append_power_5c(0x01, solarPowerW);
    append_power_5c(0x02, loadPowerW);
    append_power_5c(0x03, sitePowerW);
  } else {
    append_power_5c(0x04, batteryPowerW);
    append_u8(serviceData, BTHOME_OBJ_BOOLEAN);
    append_u8(serviceData, gridConnected ? 1 : 0);
  }
  frameFlip = !frameFlip;

  // Prepare advertisement
  NimBLEAdvertisementData advData;
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setServiceData(NimBLEUUID(BTHOME_SERVICE_UUID_16), serviceData);

  NimBLEAdvertisementData scanResp;
  if (deviceName.length()) {
    scanResp.setName(deviceName.c_str());
  }

  advertising->setAdvertisementData(advData);
  advertising->setScanResponseData(scanResp);
}


