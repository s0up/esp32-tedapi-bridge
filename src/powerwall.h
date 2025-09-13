#ifndef POWERWALL_H
#define POWERWALL_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <vector>

// TEDAPI Protocol Constants
#define TEDAPI_HOST "192.168.91.1"
#define TEDAPI_PORT 443
#define TEDAPI_TIMEOUT 10000

struct PowerwallData {
  float battery_level = 0.0f;
  float energy_remaining = 0.0f;
  float total_pack_energy = 0.0f;
  bool data_valid = false;
  unsigned long last_update = 0;
};

// Compact snapshot tailored for home automation integrations
struct HomeAutomationData {
  bool valid = false;
  float battery_percent = 0.0f;           // 0-100
  float battery_wh_remaining = 0.0f;      // Wh
  float battery_wh_full = 0.0f;           // Wh
  float site_power_w = 0.0f;              // grid import(+)/export(-) as provided
  float load_power_w = 0.0f;              // house consumption
  float solar_power_w = 0.0f;             // solar production
  float battery_power_w = 0.0f;           // battery discharge(+)/charge(-) as provided
  bool grid_connected = false;            // from control.islanding
  String island_mode;                     // BACKUP/SELF_CONSUMPTION/etc when available
  unsigned long last_update_ms = 0;       // millis()
};

class Powerwall {
private:
  const char* ssid;
  const char* gw_pwd;
  bool wifiConnected = false;
  PowerwallData currentData;
  HomeAutomationData haData;
  WiFiClientSecure client;
  String din;
  bool multiplePowerwalls = false;
  // Optional runtime/provisioned TEDAPI code override to avoid hardcoding
  std::vector<uint8_t> authCodeOverride;
  bool useAuthOverride = false;
  // Connection maintenance/backoff
  unsigned long lastWifiAttemptMs = 0;
  unsigned long wifiBackoffMs = 0;
  unsigned long lastDINFetchMs = 0;
  // Reusable buffers to avoid heap churn
  std::vector<uint8_t> requestBuffer;
  std::vector<uint8_t> responseBuffer;
  
  bool connectToWiFi();
  bool connectTEDAPI();
  bool getDIN();
  bool sendProtobufRequest(const uint8_t* data, size_t len, uint8_t* response, size_t responseCapacity, size_t* responseLen);
  bool sendProtobufRequestTo(const char* path, const uint8_t* data, size_t len, uint8_t* response, size_t responseCapacity, size_t* responseLen);
      bool getStatus();
    bool getBatteryData();
    bool getConfig();
    bool requestFirmware();
    void parseStatusData(const uint8_t* data, size_t len);
    bool parseBatteryData(const uint8_t* data, size_t len);
    bool loadAuthCodeOverrideFromConfig();

public:
  Powerwall(const char* wifiSSID, const char* gatewayPassword);
  bool begin();
  void maintain();
  PowerwallData getData();
  HomeAutomationData getHomeData();
  bool isConnected();
  void printBatteryLevel();
  bool fetchBatteryLevel();
};

#endif // POWERWALL_H 