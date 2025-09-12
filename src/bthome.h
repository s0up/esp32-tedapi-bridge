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
  void updateBatteryPercent(uint8_t batteryPercent);
  void updateBatteryAndPowers(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t sitePowerW, bool gridConnected);

private:
  void startAdvertising();
  void buildAdvertisement(uint8_t batteryPercent);
  void buildAdvertisement(uint8_t batteryPercent, int32_t solarPowerW, int32_t loadPowerW, int32_t sitePowerW, bool gridConnected);

  uint8_t lastBatteryPercent;
  bool started;
  NimBLEAdvertising *advertising;
  String deviceName;
  bool frameFlip; // alternate payloads to fit size limits
};

#endif // BTHOME_H


