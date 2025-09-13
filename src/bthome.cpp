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
  : started(false), advertising(nullptr), deviceName(""), frameIndex(0), hasData(false),
    lastAdvMs(0), advIntervalMs(1000),
    cachedBatteryPercent(0), cachedSolarW(0), cachedLoadW(0), cachedBatteryW(0), cachedSiteW(0), cachedGrid(false) {}

void BTHomeAdvertiser::begin(const String &deviceNameArg) {
  if (started) return;
  deviceName = deviceNameArg;
  NimBLEDevice::init(deviceName.c_str());
  // Increase TX power for visibility
  NimBLEDevice::setPower(ESP_PWR_LVL_P7);
  advertising = NimBLEDevice::getAdvertising();
  started = true;
  lastAdvMs = millis();
  Serial.println("[BTHome] Advertiser initialized; waiting for first data update");
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
  cachedBatteryPercent = batteryPercent;
  cachedSolarW = solarPowerW;
  cachedLoadW = loadPowerW;
  cachedBatteryW = batteryPowerW;
  cachedSiteW = sitePowerW;
  cachedGrid = gridConnected;
  hasData = true;
  // Increment packet id on new data
  packetId++;
}

void BTHomeAdvertiser::startAdvertising() {
  if (!advertising) return;
  advertising->stop();
  // Use scannable non-connectable advertising so scanners can read scan response
  advertising->setAdvertisementType(BLE_HCI_ADV_TYPE_ADV_SCAN_IND);
  advertising->setMinInterval(0x00A0); // 100ms
  advertising->setMaxInterval(0x00F0); // 150ms
  advertising->setScanResponse(true);
  advertising->start();
}

void BTHomeAdvertiser::tick() {
  if (!started || !advertising || !hasData) return;
  unsigned long now = millis();
  if (now - lastAdvMs < advIntervalMs) return;
  lastAdvMs = now;
  buildAdvertisement(cachedBatteryPercent, cachedSolarW, cachedLoadW, cachedBatteryW, cachedSiteW, cachedGrid);
  startAdvertising();
}

void BTHomeAdvertiser::buildAdvertisement(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t batteryPowerW, int32_t sitePowerW, bool gridConnected) {
  // Compact consistent frame: battery% (0x01) + four powers (0x5C): solar, load, site, battery
  std::string serviceData;
  serviceData.reserve(30);
  append_u8(serviceData, BTHOME_INFO_UNENCRYPTED_V2);

  // Battery percent (keep first, ascending ID)
  append_u8(serviceData, BTHOME_OBJ_BATTERY);
  append_u8(serviceData, batteryPercent);

  auto append_power_s32_value = [&](int32_t watts) {
    int64_t scaled = (int64_t)watts * 100LL; // factor 0.01 W
    if (scaled > INT32_MAX) scaled = INT32_MAX;
    if (scaled < INT32_MIN) scaled = INT32_MIN;
    append_u8(serviceData, BTHOME_OBJ_POWER_32);
    append_s32(serviceData, (int32_t)scaled);
  };

  // Always include all four powers in the same order so HA maps power_1..power_4 consistently
  append_power_s32_value(solarPowerW);
  append_power_s32_value(loadPowerW);
  append_power_s32_value(sitePowerW);
  append_power_s32_value(batteryPowerW);

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


