#ifndef BTHOME_H
#define BTHOME_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "powerwall.h"

// Simple BTHome v2 unencrypted advertiser for Home Assistant discovery.
// Note: We start with battery percent only; additional fields can be added later.

class BTHomeAdvertiser {
public:
  BTHomeAdvertiser();
  void begin(const String &deviceNameArg);
  void updateBatteryAndPowers(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t batteryPowerW, int32_t sitePowerW, bool gridConnected);
  void tick();

private:
  void startAdvertising();
  void buildAdvertisement(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t batteryPowerW, int32_t sitePowerW, bool gridConnected);

  bool started;
  NimBLEAdvertising *advertising;
  String deviceName;
  uint8_t frameIndex; // 0, 1 for 2-frame round-robin
  bool hasData;
  unsigned long lastAdvMs;
  unsigned long advIntervalMs;
  // Cached values for round-robin advertising
  uint8_t cachedBatteryPercent;
  int32_t cachedSolarW;
  int32_t cachedLoadW;
  int32_t cachedBatteryW;
  int32_t cachedSiteW;
  bool cachedGrid;
  // BTHome packet id (0x00) to assist deduplication
  uint8_t packetId;
};

#endif // BTHOME_H


