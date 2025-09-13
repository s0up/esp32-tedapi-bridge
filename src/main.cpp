#include <Arduino.h>
#include "powerwall.h"
#include "config.h"
#include "display.h"
#include "bthome.h"

Powerwall* powerwall;
Display* displayUI;
BTHomeAdvertiser* bthome;

void setup() {
  Serial.begin(115200);
  Serial.println("=== STARTING UP ===");

  powerwall = new Powerwall(POWERWALL_WIFI_SSID, POWERWALL_WIFI_PASSWORD);

  displayUI = new Display();
  displayUI->begin();
  displayUI->showBoot();

  bthome = new BTHomeAdvertiser();
  bthome->begin("PW BTHome");

  Serial.println("=== SETUP COMPLETE ===");
}

void loop() {
  static unsigned long lastDebug = 0;
  
  // Debug output
  if (lastDebug == 0 || millis() - lastDebug > 20000) {
    Serial.println("Loop running...");

    if (powerwall->fetchBatteryLevel()) {
      Serial.println("Successfully fetched battery data");
    } else {
      Serial.println("Failed to fetch battery data");
    }
    powerwall->printBatteryLevel();
    if (displayUI) {
      displayUI->render(powerwall->getData(), powerwall->getHomeData(), powerwall->isConnected());
    }
    // Publish BTHome battery percent + solar power if valid
    HomeAutomationData ha = powerwall->getHomeData();
    if (ha.valid && bthome) {
      uint8_t pct = (ha.battery_percent < 0) ? 0 : (ha.battery_percent > 100 ? 100 : (uint8_t)ha.battery_percent);
      int32_t solarW = (int32_t)ha.solar_power_w;
      int32_t loadW = (int32_t)ha.load_power_w;
      int32_t siteW = (int32_t)ha.site_power_w;
      int32_t battW = (int32_t)ha.battery_power_w;
      bool grid = ha.grid_connected;
      bthome->updateBatteryAndPowers(pct, solarW, loadW, battW, siteW, grid);
    }
    lastDebug = millis();
  }

  // Continuous maintenance (WiFi/DIN)
  powerwall->maintain();
  // BLE advertiser frame alternation at ~1Hz
  if (bthome) bthome->tick();
}