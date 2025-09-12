#include "powerwall.h"
#include <vector>

// Forward declaration for varint encoder used below
static size_t encodeVarint(uint8_t* buffer, uint32_t value);
// Forward declarations for protobuf readers used before their definitions
static String extractRecvTextFromQueryType(const uint8_t* p, const uint8_t* end);
static String extractRecvTextFromMessage(const uint8_t* p, const uint8_t* end);
static bool extractConfigCodeFromMessage(const uint8_t* p, const uint8_t* end, std::vector<uint8_t>& outCode);

Powerwall::Powerwall(const char* wifiSSID, const char* gatewayPassword) {
  ssid = wifiSSID;
  gw_pwd = gatewayPassword;
}

bool Powerwall::begin() {
  Serial.println("Initializing Powerwall TEDAPI connection...");
  if (!connectToWiFi()) {
    return false;
  }
  return connectTEDAPI();
}

bool Powerwall::connectToWiFi() {
  Serial.printf("Connecting to Powerwall WiFi: %s\n", ssid);
  
  WiFi.begin(ssid, gw_pwd);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    wifiConnected = false;
    Serial.println("\nFailed to connect to WiFi");
    return false;
  }
}

bool Powerwall::connectTEDAPI() {
  Serial.println("Connecting to TEDAPI...");
  
  client.setInsecure();
  
  if (!client.connect(TEDAPI_HOST, TEDAPI_PORT)) {
    Serial.println("Failed to connect to TEDAPI host");
    return false;
  }
  
  Serial.println("TEDAPI connection established");
  
  // Get DIN first (required for TEDAPI communication)
  if (!getDIN()) {
    Serial.println("Failed to get DIN from TEDAPI");
    return false;
  }
  
  Serial.printf("Successfully connected to TEDAPI with DIN: %s\n", din.c_str());
  return true;
}

bool Powerwall::getDIN() {
  Serial.println("Fetching DIN from TEDAPI...");
  
  // Create Basic Auth header: base64("Tesla_Energy_Device:gateway_password")
  String auth = "Tesla_Energy_Device:" + String(gw_pwd);
  String authEncoded = base64::encode(auth);
  
  String request = String("GET /tedapi/din HTTP/1.1\r\n") +
                  "Host: " + TEDAPI_HOST + "\r\n" +
                  "Authorization: Basic " + authEncoded + "\r\n" +
                  "Connection: keep-alive\r\n\r\n";
  
  client.print(request);
  
  // Read response
  unsigned long timeout = millis() + TEDAPI_TIMEOUT;
  String response = "";
  
  while (millis() < timeout) {
    if (client.available()) {
      response += client.readString();
      break;
    }
    delay(10);
  }
  
  // Read DIN response; avoid logging full headers
  
  // Extract DIN from response body
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart >= 0) {
    din = response.substring(bodyStart + 4);
    din.trim();
    if (din.length() > 0) {
      Serial.printf("Got DIN: %s\n", din.c_str());
      return true;
    }
  }
  
  Serial.println("Failed to extract DIN from response");
  return false;
}

bool Powerwall::sendProtobufRequest(const uint8_t* data, size_t len, uint8_t* response, size_t responseCapacity, size_t* responseLen) {
  return sendProtobufRequestTo("/tedapi/v1", data, len, response, responseCapacity, responseLen);
}

bool Powerwall::sendProtobufRequestTo(const char* path, const uint8_t* data, size_t len, uint8_t* response, size_t responseCapacity, size_t* responseLen) {
  if (!client.connected()) {
    if (!client.connect(TEDAPI_HOST, TEDAPI_PORT)) {
      Serial.println("Failed to reconnect to TEDAPI");
      return false;
    }
  }
  
  // Build HTTP header. Gateway accepts Basic auth for TEDAPI posts on some firmwares; include it to avoid 403.
  String auth = "Tesla_Energy_Device:" + String(gw_pwd);
  String authEncoded = base64::encode(auth);
  String header = String("POST ") + String(path) + String(" HTTP/1.1\r\n") +
                  "Host: " + TEDAPI_HOST + "\r\n" +
                  "Authorization: Basic " + authEncoded + "\r\n" +
                  "Content-Type: application/octet-string\r\n" +
                  "Content-Length: " + String(len) + "\r\n" +
                  "Connection: close\r\n\r\n";  // MATCH PYTHON: use Connection: close
  
  // Send request
  client.print(header);
  client.write(data, len);
  client.flush();
  
  // Read HTTP response
  unsigned long timeout = millis() + TEDAPI_TIMEOUT;
  String httpResponse = "";
  
  while (millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      httpResponse += c;
      
      if (httpResponse.endsWith("\r\n\r\n")) {
        break;
      }
    } else {
      delay(10);
    }
  }
  
  // Headers read complete
  
  // Check for 200 OK
  if (httpResponse.indexOf("200 OK") < 0) {
    Serial.println("TEDAPI request failed - not 200 OK");
    return false;
  }
  
  // Handle chunked encoding or content-length
  bool isChunked = httpResponse.indexOf("Transfer-Encoding: chunked") >= 0;
  int contentLength = 0;
  
  if (!isChunked) {
    int clStart = httpResponse.indexOf("Content-Length: ");
    if (clStart >= 0) {
      clStart += 16;
      int clEnd = httpResponse.indexOf("\r\n", clStart);
      if (clEnd >= 0) {
        contentLength = httpResponse.substring(clStart, clEnd).toInt();
      }
    }
  }
  
  // Read protobuf body
  size_t bytesRead = 0;
  timeout = millis() + TEDAPI_TIMEOUT;
  
  if (isChunked) {
    // Handle chunked encoding
    while (millis() < timeout && bytesRead < responseCapacity) {
      if (client.available()) {
        // Read chunk size line
        String chunkSizeLine = client.readStringUntil('\n');
        chunkSizeLine.trim();
        if (chunkSizeLine.length() == 0) continue;
        
        // Parse hex chunk size
        long chunkSize = strtol(chunkSizeLine.c_str(), nullptr, 16);
        if (chunkSize == 0) break; // End of chunks
        
        // Read chunk data
        while (chunkSize > 0 && bytesRead < responseCapacity && millis() < timeout) {
          if (client.available()) {
            response[bytesRead++] = client.read();
            chunkSize--;
          } else {
            delay(1);
          }
        }
        
        // Skip trailing CRLF after chunk
        if (client.available()) client.read(); // \r
        if (client.available()) client.read(); // \n
      } else {
        delay(10);
      }
    }
  } else {
    // Handle content-length
    while (millis() < timeout && bytesRead < contentLength && bytesRead < responseCapacity) {
      if (client.available()) {
        response[bytesRead++] = client.read();
      } else {
        delay(10);
      }
    }
  }
  
  *responseLen = bytesRead;
  // Body read complete; close connection since we requested Connection: close
  client.stop();
  
  return bytesRead > 0;
}

bool Powerwall::getStatus() {
  // Only fetch battery status; config and firmware are not required for battery level
  return getBatteryData();
}

bool Powerwall::getConfig() {
  Serial.println("Requesting config...");
  if (din.isEmpty()) return false;

  uint8_t req[256];
  size_t len = 0;

  // Build minimal config request like Python: delivery=1, sender.local=1, recipient.din, config.send{num=1,file="config.json"}, tail=1
  String file = "config.json";
  size_t fileLen = file.length();

  // Envelope sizes
  size_t sendSize = 1 + 1 + // num=1
                    1 + 1 + fileLen; // file
  size_t configSize = 1 + encodeVarint(nullptr, sendSize) + sendSize;
  size_t recipientSize = 1 + encodeVarint(nullptr, din.length()) + din.length();
  size_t senderSize = 1 + 1;
  size_t envelopeSize = 1 + 1 +
                        1 + encodeVarint(nullptr, senderSize) + senderSize +
                        1 + encodeVarint(nullptr, recipientSize) + recipientSize +
                        1 + encodeVarint(nullptr, configSize) + configSize;

  // message
  req[len++] = 0x0A; // field 1
  len += encodeVarint(&req[len], envelopeSize);
  // deliveryChannel=1
  req[len++] = 0x08; req[len++] = 0x01;
  // sender
  req[len++] = 0x12; len += encodeVarint(&req[len], senderSize); req[len++] = 0x18; req[len++] = 0x01;
  // recipient
  req[len++] = 0x1A; len += encodeVarint(&req[len], recipientSize); req[len++] = 0x0A; len += encodeVarint(&req[len], din.length()); memcpy(&req[len], din.c_str(), din.length()); len += din.length();
  // config (field 15)
  req[len++] = 0x7A; // 15<<3 | 2 = 120 -> 0x78? Actually field 15 tag is 0x7A with sub-structure start
  len += encodeVarint(&req[len], configSize);
  // config.send (field 1)
  req[len++] = 0x0A; len += encodeVarint(&req[len], sendSize);
  // num=1
  req[len++] = 0x08; req[len++] = 0x01;
  // file
  req[len++] = 0x12; req[len++] = fileLen; memcpy(&req[len], file.c_str(), fileLen); len += fileLen;
  // tail (root message field 2) value=1
  {
    size_t tailSize = 1 + 1; // Tail.value (field 1 varint) + value 1
    req[len++] = 0x12; // Message field 2 (tail), wire type 2
    len += encodeVarint(&req[len], tailSize);
    req[len++] = 0x08; // Tail.value field 1, varint
    req[len++] = 0x01; // value = 1
  }

  uint8_t response[4096]; size_t responseLen;
  if (!sendProtobufRequest(req, len, response, sizeof(response), &responseLen)) return false;
  // Minimal logging: skip full hex dump
  // Try to find a '{' JSON config and detect number of powerwalls
  for (size_t i = 0; i < responseLen; i++) {
    if (response[i] == '{') {
      String json; int braces=0; bool start=false;
      for (size_t j=i; j<responseLen; j++) { char c=(char)response[j]; if (c=='{'){braces++; start=true;} if (start) json+=c; if (c=='}'){braces--; if (braces==0) break; } }
      Serial.println("Config JSON found:"); Serial.println(json);
      DynamicJsonDocument doc(8192);
      if (deserializeJson(doc, json) == DeserializationError::Ok) {
        // Detect multiple Powerwalls from config.json top-level "battery_blocks"
        JsonVariant blocks = doc["battery_blocks"];
        if (!blocks.isNull() && blocks.is<JsonArray>()) {
          JsonArray arr = blocks.as<JsonArray>();
          multiplePowerwalls = arr.size() > 1;
          Serial.printf("Detected multiple Powerwalls: %s\n", multiplePowerwalls ? "yes" : "no");
        }
      }
      break;
    }
  }
  // Additionally, try to extract the config.recv.code (TEDAPI auth code) from the protobuf
  std::vector<uint8_t> code;
  if (extractConfigCodeFromMessage(response, response + responseLen, code) && !code.empty()) {
    authCodeOverride = code; useAuthOverride = true;
    Serial.printf("Extracted TEDAPI code from config response (%d bytes): ", (int)code.size());
    for (size_t i = 0; i < code.size() && i < 20; i++) {
      Serial.printf("%02X ", code[i]);
    }
    Serial.println();
  }
  return true;
}

void Powerwall::parseStatusData(const uint8_t* data, size_t len) {
  Serial.printf("Parsing %d bytes of protobuf response\n", len);
  
  // Print full hex dump for debugging
  Serial.print("Full protobuf response: ");
  for (size_t i = 0; i < len; i++) {
    Serial.printf("%02X ", data[i]);
    if (i % 16 == 15) Serial.println();
  }
  if (len % 16 != 0) Serial.println();
  
  // Convert bytes to ASCII to see if there's readable text
  Serial.print("ASCII representation: ");
  for (size_t i = 0; i < len; i++) {
    if (data[i] >= 32 && data[i] <= 126) {
      Serial.printf("%c", data[i]);
    } else {
      Serial.print(".");
    }
  }
  Serial.println();
  
  Serial.println("Firmware response received - authentication successful!");
}

bool Powerwall::fetchBatteryLevel() {
  if (!wifiConnected) {
    return false;
  }
  
  return getStatus();
}

PowerwallData Powerwall::getData() {
  return currentData;
}

HomeAutomationData Powerwall::getHomeData() {
  return haData;
}

bool Powerwall::isConnected() {
  return wifiConnected && WiFi.status() == WL_CONNECTED && !din.isEmpty();
}

void Powerwall::printBatteryLevel() {
  if (currentData.data_valid) {
    Serial.printf("Powerwall Battery: %.1f%% (TEDAPI)\n", currentData.battery_level);
  } else {
    Serial.println("No valid Powerwall data available");
  }
} 

// Helper function to encode varint
static size_t encodeVarint(uint8_t* buffer, uint32_t value) {
  size_t len = 0;
  while (value >= 0x80) {
    if (buffer) buffer[len] = (value & 0xFF) | 0x80;
    len++;
    value >>= 7;
  }
  if (buffer) buffer[len] = value & 0xFF;
  len++;
  return len;
}

// --- Minimal protobuf reader helpers (varint, skip, and targeted recv.text extractor) ---
static bool readVarint(const uint8_t*& p, const uint8_t* end, uint32_t& out) {
  uint32_t result = 0;
  int shift = 0;
  while (p < end && shift <= 28) {
    uint8_t b = *p++;
    result |= uint32_t(b & 0x7F) << shift;
    if ((b & 0x80) == 0) { out = result; return true; }
    shift += 7;
  }
  return false;
}

static bool skipField(const uint8_t*& p, const uint8_t* end, uint8_t wireType) {
  switch (wireType) {
    case 0: { // varint
      uint32_t tmp; return readVarint(p, end, tmp);
    }
    case 1: { // 64-bit
      if (end - p < 8) return false; p += 8; return true;
    }
    case 2: { // length-delimited
      uint32_t len; if (!readVarint(p, end, len)) return false; if (uint32_t(end - p) < len) return false; p += len; return true;
    }
    case 5: { // 32-bit
      if (end - p < 4) return false; p += 4; return true;
    }
    default: return false;
  }
}

static String extractRecvTextFromQueryType(const uint8_t* p, const uint8_t* end);
static String extractRecvTextFromMessage(const uint8_t* p, const uint8_t* end) {
  // message fields: 1: message (envelope), 16: payload (QueryType)
  while (p < end) {
    uint32_t key; if (!readVarint(p, end, key)) break;
    uint8_t wt = key & 0x07; uint32_t fn = key >> 3;
    if (wt == 2) {
      uint32_t len; if (!readVarint(p, end, len)) break; if (uint32_t(end - p) < len) break;
      const uint8_t* subEnd = p + len;
      if (fn == 1) {
        // envelope submessage; scan inside for payload
        String s = extractRecvTextFromMessage(p, subEnd);
        if (s.length()) return s;
      } else if (fn == 16) {
        // QueryType
        String s = extractRecvTextFromQueryType(p, subEnd);
        if (s.length()) return s;
      }
      p = subEnd;
    } else {
      if (!skipField(p, end, wt)) break;
    }
  }
  return String();
}

// Extract Config.recv.code bytes from message
static bool extractConfigCodeFromConfigType(const uint8_t* p, const uint8_t* end, std::vector<uint8_t>& outCode) {
  while (p < end) {
    uint32_t key; if (!readVarint(p, end, key)) break;
    uint8_t wt = key & 0x07; uint32_t fn = key >> 3;
    if (fn == 2 && wt == 2) { // recv
      uint32_t len; if (!readVarint(p, end, len)) break; if ((uint32_t)(end - p) < len) break;
      const uint8_t* sub = p; const uint8_t* subEnd = p + len;
      // PayloadConfigRecv: 1 file (ConfigString), 2 code (bytes)
      while (sub < subEnd) {
        uint32_t k2; if (!readVarint(sub, subEnd, k2)) break;
        uint8_t wt2 = k2 & 0x07; uint32_t fn2 = k2 >> 3;
        if (fn2 == 2 && wt2 == 2) { // code bytes
          uint32_t bl; if (!readVarint(sub, subEnd, bl)) break; if ((uint32_t)(subEnd - sub) < bl) break;
          outCode.assign(sub, sub + bl);
          return true;
        } else {
          if (!skipField(sub, subEnd, wt2)) break;
        }
      }
      p = subEnd;
    } else {
      if (!skipField(p, end, wt)) break;
    }
  }
  return false;
}

static bool extractConfigCodeFromEnvelope(const uint8_t* p, const uint8_t* end, std::vector<uint8_t>& outCode) {
  // MessageEnvelope fields include 15: config (ConfigType)
  while (p < end) {
    uint32_t key; if (!readVarint(p, end, key)) break;
    uint8_t wt = key & 0x07; uint32_t fn = key >> 3;
    if (fn == 15 && wt == 2) { // config
      uint32_t len; if (!readVarint(p, end, len)) break; if ((uint32_t)(end - p) < len) break;
      const uint8_t* subEnd = p + len;
      bool ok = extractConfigCodeFromConfigType(p, subEnd, outCode);
      if (ok) return true;
      p = subEnd;
    } else if (wt == 2) {
      uint32_t len; if (!readVarint(p, end, len)) break; if ((uint32_t)(end - p) < len) break; p += len;
    } else {
      if (!skipField(p, end, wt)) break;
    }
  }
  return false;
}

static bool extractConfigCodeFromMessage(const uint8_t* p, const uint8_t* end, std::vector<uint8_t>& outCode) {
  // message field 1: envelope
  while (p < end) {
    uint32_t key; if (!readVarint(p, end, key)) break;
    uint8_t wt = key & 0x07; uint32_t fn = key >> 3;
    if (fn == 1 && wt == 2) {
      uint32_t len; if (!readVarint(p, end, len)) break; if ((uint32_t)(end - p) < len) break;
      const uint8_t* subEnd = p + len;
      bool ok = extractConfigCodeFromEnvelope(p, subEnd, outCode);
      if (ok) return true;
      p = subEnd;
    } else {
      if (!skipField(p, end, wt)) break;
    }
  }
  return false;
}

static String extractRecvTextFromPayloadString(const uint8_t* p, const uint8_t* end) {
  // PayloadString: 1:value (varint), 2:text (string)
  while (p < end) {
    uint32_t key; if (!readVarint(p, end, key)) break;
    uint8_t wt = key & 0x07; uint32_t fn = key >> 3;
    if (fn == 2 && wt == 2) {
      uint32_t len; if (!readVarint(p, end, len)) break; if (uint32_t(end - p) < len) break;
      String s; s.reserve(len);
      for (uint32_t i = 0; i < len; i++) s += char(p[i]);
      return s;
    } else {
      if (!skipField(p, end, wt)) break;
    }
  }
  return String();
}

static String extractRecvTextFromQueryType(const uint8_t* p, const uint8_t* end) {
  // QueryType: 1: send, 2: recv (PayloadString)
  while (p < end) {
    uint32_t key; if (!readVarint(p, end, key)) break;
    uint8_t wt = key & 0x07; uint32_t fn = key >> 3;
    if (fn == 2 && wt == 2) {
      uint32_t len; if (!readVarint(p, end, len)) break; if (uint32_t(end - p) < len) break;
      const uint8_t* subEnd = p + len;
      String s = extractRecvTextFromPayloadString(p, subEnd);
      return s;
    } else {
      if (!skipField(p, end, wt)) break;
    }
  }
  return String();
}

bool Powerwall::getBatteryData() {
  Serial.println("Requesting battery data from TEDAPI...");
  
  if (din.isEmpty()) {
    Serial.println("No DIN available - cannot request battery data");
    return false;
  }
  
  // Use existing connection to reduce memory churn
  // Using existing connection
  
  // GraphQL query MUST MATCH the Python reference exactly for the precomputed signature to validate
  const char* graphqlQuery = R"( query DeviceControllerQuery {
  control {
    systemStatus {
        nominalFullPackEnergyWh
        nominalEnergyRemainingWh
    }
    islanding {
        customerIslandMode
        contactorClosed
        microGridOK
        gridOK
    }
    meterAggregates {
      location
      realPowerW
    }
    alerts {
      active
    },
    siteShutdown {
      isShutDown
      reasons
    }
    batteryBlocks {
      din
      disableReasons
    }
    pvInverters {
      din
      disableReasons
    }
  }
  system {
    time
    sitemanagerStatus {
      isRunning
    }
    updateUrgencyCheck  {
      urgency
      version {
        version
        gitHash
      }
      timestamp
    }
  }
  neurio {
    isDetectingWiredMeters
    readings {
      serial
      dataRead {
        voltageV
        realPowerW
        reactivePowerVAR
        currentA
      }
      timestamp
    }
    pairings {
      serial
      shortId
      status
      errors
      macAddress
      isWired
      modbusPort
      modbusId
      lastUpdateTimestamp
    }
  }
  pw3Can {
    firmwareUpdate {
      isUpdating
      progress {
         updating
         numSteps
         currentStep
         currentStepProgress
         progress
      }
    }
  }
  esCan {
    bus {
      PVAC {
        packagePartNumber
        packageSerialNumber
        subPackagePartNumber
        subPackageSerialNumber
        PVAC_Status {
          isMIA
          PVAC_Pout
          PVAC_State
          PVAC_Vout
          PVAC_Fout
        }
        PVAC_InfoMsg {
          PVAC_appGitHash
        }
        PVAC_Logging {
          isMIA
          PVAC_PVCurrent_A
          PVAC_PVCurrent_B
          PVAC_PVCurrent_C
          PVAC_PVCurrent_D
          PVAC_PVMeasuredVoltage_A
          PVAC_PVMeasuredVoltage_B
          PVAC_PVMeasuredVoltage_C
          PVAC_PVMeasuredVoltage_D
          PVAC_VL1Ground
          PVAC_VL2Ground
        }
        alerts {
          isComplete
          isMIA
          active
        }
      }
      PINV {
        PINV_Status {
          isMIA
          PINV_Fout
          PINV_Pout
          PINV_Vout
          PINV_State
          PINV_GridState
        }
        PINV_AcMeasurements {
          isMIA
          PINV_VSplit1
          PINV_VSplit2
        }
        PINV_PowerCapability {
          isComplete
          isMIA
          PINV_Pnom
        }
        alerts {
          isComplete
          isMIA
          active
        }
      }
      PVS {
        PVS_Status {
          isMIA
          PVS_State
          PVS_vLL
          PVS_StringA_Connected
          PVS_StringB_Connected
          PVS_StringC_Connected
          PVS_StringD_Connected
          PVS_SelfTestState
        }
        alerts {
          isComplete
          isMIA
          active
        }
      }
      THC {
        packagePartNumber
        packageSerialNumber
        THC_InfoMsg {
          isComplete
          isMIA
          THC_appGitHash
        }
        THC_Logging {
          THC_LOG_PW_2_0_EnableLineState
        }
      }
      POD {
        POD_EnergyStatus {
          isMIA
          POD_nom_energy_remaining
          POD_nom_full_pack_energy
        }
        POD_InfoMsg {
            POD_appGitHash
        }
      }
      MSA {
        packagePartNumber
        packageSerialNumber
        MSA_InfoMsg {
          isMIA
          MSA_appGitHash
          MSA_assemblyId
        }
        METER_Z_AcMeasurements {
          isMIA
          lastRxTime
          METER_Z_CTA_InstRealPower
          METER_Z_CTA_InstReactivePower
          METER_Z_CTA_I
          METER_Z_VL1G
          METER_Z_CTB_InstRealPower
          METER_Z_CTB_InstReactivePower
          METER_Z_CTB_I
          METER_Z_VL2G
        }
        MSA_Status {
          lastRxTime
        }
      }
      SYNC {
        packagePartNumber
        packageSerialNumber
        SYNC_InfoMsg {
          isMIA
          SYNC_appGitHash
        }
        METER_X_AcMeasurements {
          isMIA
          isComplete
          lastRxTime
          METER_X_CTA_InstRealPower
          METER_X_CTA_InstReactivePower
          METER_X_CTA_I
          METER_X_VL1N
          METER_X_CTB_InstRealPower
          METER_X_CTB_InstReactivePower
          METER_X_CTB_I
          METER_X_VL2N
          METER_X_CTC_InstRealPower
          METER_X_CTC_InstReactivePower
          METER_X_CTC_I
          METER_X_VL3N
        }
        METER_Y_AcMeasurements {
          isMIA
          isComplete
          lastRxTime
          METER_Y_CTA_InstRealPower
          METER_Y_CTA_InstReactivePower
          METER_Y_CTA_I
          METER_Y_VL1N
          METER_Y_CTB_InstRealPower
          METER_Y_CTB_InstReactivePower
          METER_Y_CTB_I
          METER_Y_VL2N
          METER_Y_CTC_InstRealPower
          METER_Y_CTC_InstReactivePower
          METER_Y_CTC_I
          METER_Y_VL3N
        }
        SYNC_Status {
          lastRxTime
        }
      }
      ISLANDER {
        ISLAND_GridConnection {
          ISLAND_GridConnected
          isComplete
        }
        ISLAND_AcMeasurements {
          ISLAND_VL1N_Main
          ISLAND_FreqL1_Main
          ISLAND_VL2N_Main
          ISLAND_FreqL2_Main
          ISLAND_VL3N_Main
          ISLAND_FreqL3_Main
          ISLAND_VL1N_Load
          ISLAND_FreqL1_Load
          ISLAND_VL2N_Load
          ISLAND_FreqL2_Load
          ISLAND_VL3N_Load
          ISLAND_FreqL3_Load
          ISLAND_GridState
          lastRxTime
          isComplete
          isMIA
        }
      }
    }
    enumeration {
      inProgress
      numACPW
      numPVI
    }
    firmwareUpdate {
      isUpdating
      powerwalls {
        updating
        numSteps
        currentStep
        currentStepProgress
        progress
      }
      msa {
        updating
        numSteps
        currentStep
        currentStepProgress
        progress
      }
      sync {
        updating
        numSteps
        currentStep
        currentStepProgress
        progress
      }
      pvInverters {
        updating
        numSteps
        currentStep
        currentStepProgress
        progress
      }
    }
    phaseDetection {
      inProgress
      lastUpdateTimestamp
      powerwalls {
        din
        progress
        phase
      }
    }
    inverterSelfTests {
      isRunning
      isCanceled
      pinvSelfTestsResults {
        din
        overall {
          status
          test
          summary
          setMagnitude
          setTime
          tripMagnitude
          tripTime
          accuracyMagnitude
          accuracyTime
          currentMagnitude
          timestamp
          lastError
        }
        testResults {
          status
          test
          summary
          setMagnitude
          setTime
          tripMagnitude
          tripTime
          accuracyMagnitude
          accuracyTime
          currentMagnitude
          timestamp
          lastError
        }
      }
    }
  }
}
)";
  size_t graphqlLen = strlen(graphqlQuery);
  
  // Hardcoded DER auth code used by Python reference for status query
  static const uint8_t authCodeStatus[] PROGMEM = {
    0x30,0x81,0x86,0x02,0x41,0x14,0xB1,0x97,0xA5,0x7F,0xAD,0xB5,0xBA,0xD1,0x72,0x1A,
    0xA8,0xBD,0x6A,0xC5,0x18,0x98,0x30,0xB6,0x12,0x42,0xA2,0xB4,0x70,0x4F,0xB2,0x14,
    0x76,0x64,0xB7,0xCE,0x1A,0x0C,0xFE,0xD2,0x56,0x01,0x0C,0x7F,0x2A,0xF6,0xE5,0xDB,
    0x67,0x5F,0x2F,0x60,0x0B,0x16,0x95,0x5F,0x71,0x63,0x13,0x24,0xD3,0x8E,0x79,0xBE,
    0x7E,0xDD,0x41,0x31,0x12,0x78,0x02,0x41,0x70,0x07,0x5F,0xB4,0x1F,0x5D,0xC4,0x3E,
    0xF2,0xEE,0x05,0xA5,0x56,0xC1,0x7F,0x2A,0x08,0xC7,0x0E,0xA6,0x5D,0x1F,0x82,0xA2,
    0xEB,0x49,0x7E,0xDA,0xCF,0x11,0xDE,0x06,0x1B,0x71,0xCF,0xC9,0xB4,0xCD,0xFC,0x1E,
    0xF5,0x73,0xBA,0x95,0x8D,0x23,0x6F,0x21,0xCD,0x7A,0xEB,0xE5,0x7A,0x96,0xF5,0xE1,
    0x0C,0xB5,0xAE,0x72,0xFB,0xCB,0x2F,0x17,0x1F
  };

  // Use heap allocation to avoid stack overflow; larger buffer for full Python query
  // Reduce request capacity; our assembled protobuf is ~7KB
  const size_t requestCapacity = 8192;
  uint8_t* requestBuf = (uint8_t*)malloc(requestCapacity);
  // Battery response can be large; allow up to 32KB to capture full recv.text JSON
  // Limit response capacity; typical recv.text payload < 20KB
  const size_t responseCapacity = 24576;
  uint8_t* responseBuf = (uint8_t*)malloc(responseCapacity);
  if (!requestBuf || !responseBuf) {
    Serial.println("Failed to allocate memory for buffers");
    if (requestBuf) free(requestBuf);
    if (responseBuf) free(responseBuf);
    return false;
  }
  
  // Clear buffers to prevent data leakage
  memset(requestBuf, 0, requestCapacity);
  memset(responseBuf, 0, responseCapacity);
  
  size_t len = 0;
  
  // Calculate sizes properly using varint encoding
  size_t dinLen = din.length();
  // IMPORTANT: For status query, gateway expects DER-encoded signature as in Python (137 bytes)
  // Do NOT use the 32-byte config code here; it causes "Invalid signature format".
  size_t codeLen = sizeof(authCodeStatus);
  
  // PayloadString for payload.send.payload
  size_t payloadStringSize = 1 + 1 + // value field (field + value)
                            1 + encodeVarint(nullptr, graphqlLen) + graphqlLen; // text field
  
  // PayloadQuerySend for payload.send (use actual auth code length)
  size_t payloadQuerySendSize = 1 + 1 + // num = 2 (field + value)
                               1 + encodeVarint(nullptr, payloadStringSize) + payloadStringSize + // payload
                               1 + encodeVarint(nullptr, codeLen) + codeLen + // DER auth code
                               1 + 1 + 1 + 1 + 2; // b.value = "{}" (field + len + field + len + content)
  
  // Participants  
  size_t recipientSize = 1 + encodeVarint(nullptr, din.length()) + din.length(); // recipient.din - MATCH PYTHON
  size_t senderSize = 1 + 1; // sender.local = 1 (always for status queries)
  
  // QueryType wrapper (payload field 16 contains QueryType which wraps send)
  size_t queryTypeSize = 1 /*send tag*/ + encodeVarint(nullptr, payloadQuerySendSize) + payloadQuerySendSize;

  // MessageEnvelope
  size_t envelopeSize = 1 + 1 + // deliveryChannel = 1  
                       1 + encodeVarint(nullptr, senderSize) + senderSize + // sender
                       1 + encodeVarint(nullptr, recipientSize) + recipientSize + // recipient
                       2 /*field 16 tag*/ + encodeVarint(nullptr, queryTypeSize) + queryTypeSize; // payload (QueryType)
  
  // Root message (field 1)
  requestBuf[len++] = 0x0A; // field 1, wire type 2
  len += encodeVarint(&requestBuf[len], envelopeSize);
  
  // deliveryChannel = 1 (field 1)
  requestBuf[len++] = 0x08; // field 1, wire type 0
  requestBuf[len++] = 0x01; // value 1
  
  // sender (field 2) - always use sender.local = 1 for status queries like Python
  requestBuf[len++] = 0x12; // field 2, wire type 2
  len += encodeVarint(&requestBuf[len], senderSize);
  requestBuf[len++] = 0x18; // field 3, wire type 0 (local)
  requestBuf[len++] = 0x01; // value 1
  
  // recipient (field 3) - MATCH PYTHON: use recipient.din (NOT local!)
  requestBuf[len++] = 0x1A; // field 3, wire type 2
  len += encodeVarint(&requestBuf[len], recipientSize);
  requestBuf[len++] = 0x0A; // field 1, wire type 2 (din)
  len += encodeVarint(&requestBuf[len], din.length());
  memcpy(&requestBuf[len], din.c_str(), din.length());
  len += din.length();
  
  // payload (field 16 = QueryType)
  requestBuf[len++] = 0x82; // field 16, wire type 2 (128 + 2 = 130 = 0x82)
  requestBuf[len++] = 0x01; // continuation byte for varint key
  len += encodeVarint(&requestBuf[len], queryTypeSize);

  // QueryType.send (field 1)
  requestBuf[len++] = 0x0A; // field 1 (send), wire type 2
  len += encodeVarint(&requestBuf[len], payloadQuerySendSize);

  // send.num = 2 (field 1)
  requestBuf[len++] = 0x08; // field 1, wire type 0
  requestBuf[len++] = 0x02; // value 2
  
  // send.payload (field 2)
  requestBuf[len++] = 0x12; // field 2, wire type 2
  len += encodeVarint(&requestBuf[len], payloadStringSize);
  requestBuf[len++] = 0x08; // field 1, wire type 0 (value)
  requestBuf[len++] = 0x01; // value 1
  requestBuf[len++] = 0x12; // field 2, wire type 2 (text)
  len += encodeVarint(&requestBuf[len], graphqlLen);
  memcpy(&requestBuf[len], graphqlQuery, graphqlLen);
  len += graphqlLen;
  
  // Write auth code (field 3)
  // Using embedded DER auth code for status query
    requestBuf[len++] = 0x1A; // field 3, wire type 2
  len += encodeVarint(&requestBuf[len], codeLen);
  if (len + codeLen > requestCapacity) { Serial.println("Request overflow on auth code"); free(requestBuf); free(responseBuf); return false; }
  memcpy_P(&requestBuf[len], authCodeStatus, codeLen);
  len += codeLen;
  
  
  // send.b (field 4)
  requestBuf[len++] = 0x22; // field 4, wire type 2
  requestBuf[len++] = 0x04; // length 4
  requestBuf[len++] = 0x0A; // field 1, wire type 2 (value)
  requestBuf[len++] = 0x02; // string length 2
  requestBuf[len++] = '{'; // content
  requestBuf[len++] = '}'; // content
  
  // tail: field 2 (length-delimited), inner Tail.value = 1 or 2 - MATCH PYTHON
  requestBuf[len++] = 0x12; // field 2, wire type 2
  requestBuf[len++] = 0x02; // length of Tail message
  requestBuf[len++] = 0x08; // Tail field 1, varint
  requestBuf[len++] = 0x01; // Python uses tail.value = 1 for status
  
  
  
  // Print hex dump of our protobuf for debugging
  // Skip protobuf hex dump
  
  size_t responseLen;
  
  // Choose path based on number of devices
  const char* path = "/tedapi/v1";
  if (sendProtobufRequestTo(path, requestBuf, len, responseBuf, responseCapacity, &responseLen)) {
    // Minimal logging
    
    // Try to parse valid JSON; if not present or we see an auth error string, retry with alternate auth code
    bool hasJson = false;
    for (size_t i = 0; i < responseLen; i++) {
      if (responseBuf[i] == '{') { hasJson = true; break; }
    }
    // Detect ASCII "missing AuthEnvelo" in response
    bool authError = false;
    const char* needle = "missing AuthEnvelo";
    size_t nlen = strlen(needle);
    for (size_t i = 0; i + nlen <= responseLen; i++) {
      size_t k = 0; while (k < nlen && (char)responseBuf[i+k] == needle[k]) k++;
      if (k == nlen) { authError = true; break; }
    }
    
    // Skip ASCII dump
    
    if (hasJson && !authError) {
      parseBatteryData(responseBuf, responseLen);
      free(requestBuf);
      free(responseBuf);
      return true;
    }
    Serial.println("Battery query failed");
    free(requestBuf);
    free(responseBuf);
    return false;
  }
  free(requestBuf);
  free(responseBuf);
  return false;
}

bool Powerwall::requestFirmware() {
  Serial.println("Requesting firmware via TEDAPI...");
  if (din.isEmpty()) return false;

  uint8_t req[256]; size_t len=0;
  // Envelope sizes
  size_t recipientSize = 1 + encodeVarint(nullptr, din.length()) + din.length();
  size_t senderSize = 1 + 1; // local
  size_t firmwareSize = 1 + 1; // request="" (field 2 empty string)
  size_t envelopeSize = 1 + 1 +
                        1 + encodeVarint(nullptr, senderSize) + senderSize +
                        1 + encodeVarint(nullptr, recipientSize) + recipientSize +
                        1 + encodeVarint(nullptr, firmwareSize) + firmwareSize;
  req[len++] = 0x0A; len += encodeVarint(&req[len], envelopeSize);
  req[len++] = 0x08; req[len++] = 0x01; // delivery
  req[len++] = 0x12; len += encodeVarint(&req[len], senderSize); req[len++] = 0x18; req[len++] = 0x01; // sender.local
  req[len++] = 0x1A; len += encodeVarint(&req[len], recipientSize); req[len++] = 0x0A; len += encodeVarint(&req[len], din.length()); memcpy(&req[len], din.c_str(), din.length()); len += din.length();
  // firmware (field 4)
  req[len++] = 0x22; len += encodeVarint(&req[len], firmwareSize);
  // firmware.request (field 2) empty string
  req[len++] = 0x12; req[len++] = 0x00;
  // tail
  req[len++] = 0x10; req[len++] = 0x01;

  uint8_t response[4096]; size_t responseLen;
  if (!sendProtobufRequest(req, len, response, sizeof(response), &responseLen)) return false;
  Serial.println("Firmware response received");
  return true;
}

void Powerwall::parseBatteryData(const uint8_t* data, size_t len) {
  // Quiet parsing – rely on targeted recv.text extraction

  // Try to extract message.payload.recv.text if present
  String recvText = extractRecvTextFromMessage(data, data + len);
  if (recvText.length()) {
    // Extract the first complete JSON object from recv.text (avoid extra String copies)
    int start = recvText.indexOf('{');
    if (start < 0) return; // no JSON
    int braces = 0; bool started = false; int end = -1;
    for (int i = start; i < recvText.length(); i++) {
      char c = recvText.charAt(i);
      if (c == '{') { braces++; started = true; }
      if (started && c == '}') { braces--; if (braces == 0) { end = i; break; } }
    }
    if (end < 0) return; // incomplete JSON

    const char* jsonPtr = recvText.c_str() + start;
    size_t jsonLen = (size_t)(end - start + 1);

    // Use a filter to only parse the fields we need to reduce memory
    StaticJsonDocument<512> filter;
    // Top-level and nested under data
    JsonObject fTop = filter.createNestedObject("control");
    fTop["systemStatus"]["nominalFullPackEnergyWh"] = true;
    fTop["systemStatus"]["nominalEnergyRemainingWh"] = true;
    fTop["islanding"]["gridOK"] = true;
    fTop["islanding"]["customerIslandMode"] = true;
    fTop["meterAggregates"][0]["location"] = true;
    fTop["meterAggregates"][0]["realPowerW"] = true;
    JsonObject fData = filter.createNestedObject("data");
    JsonObject fCtrl = fData.createNestedObject("control");
    fCtrl["systemStatus"]["nominalFullPackEnergyWh"] = true;
    fCtrl["systemStatus"]["nominalEnergyRemainingWh"] = true;
    fCtrl["islanding"]["gridOK"] = true;
    fCtrl["islanding"]["customerIslandMode"] = true;
    fCtrl["meterAggregates"][0]["location"] = true;
    fCtrl["meterAggregates"][0]["realPowerW"] = true;

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, jsonPtr, jsonLen, DeserializationOption::Filter(filter));
    if (err) return;

    JsonVariant root = doc.as<JsonVariant>();
    if (doc.containsKey("data")) {
      root = doc["data"];
    }
    JsonVariant control = root["control"];
    JsonVariant systemStatus = control["systemStatus"];
    float remaining = systemStatus["nominalEnergyRemainingWh"] | 0;
    float total = systemStatus["nominalFullPackEnergyWh"] | 0;
    if (total > 0 && remaining > 0) {
      currentData.battery_level = (remaining / total) * 100.0f;
      currentData.energy_remaining = remaining;
      currentData.total_pack_energy = total;
      currentData.data_valid = true;
      currentData.last_update = millis();
      // Build HomeAutomationData snapshot
      haData.valid = true;
      haData.battery_percent = currentData.battery_level;
      haData.battery_wh_remaining = remaining;
      haData.battery_wh_full = total;
      haData.last_update_ms = currentData.last_update;
      // Grid/island state
      JsonVariant islanding = control["islanding"];
      haData.grid_connected = islanding["gridOK"] | false;
      const char* cmode = islanding["customerIslandMode"] | "";
      haData.island_mode = String(cmode);
      // Meter aggregates
      haData.site_power_w = 0;
      haData.load_power_w = 0;
      haData.solar_power_w = 0;
      haData.battery_power_w = 0;
      JsonVariant mags = control["meterAggregates"];
      if (mags.is<JsonArray>()) {
        for (JsonVariant v : mags.as<JsonArray>()) {
          const char* loc = v["location"] | "";
          float p = v["realPowerW"] | 0.0f;
          if (strcmp(loc, "SITE") == 0) haData.site_power_w = p;
          else if (strcmp(loc, "LOAD") == 0) haData.load_power_w = p;
          else if (strcmp(loc, "SOLAR") == 0) haData.solar_power_w = p;
          else if (strcmp(loc, "BATTERY") == 0) haData.battery_power_w = p;
        }
      }
      // Now print concise HA summary
      Serial.printf("HA: batt=%.1f%% rem=%.0fWh full=%.0fWh | site=%.0fW load=%.0fW solar=%.0fW battery=%.0fW | grid=%s mode=%s\n",
                    haData.battery_percent,
                    haData.battery_wh_remaining,
                    haData.battery_wh_full,
                    haData.site_power_w,
                    haData.load_power_w,
                    haData.solar_power_w,
                    haData.battery_power_w,
                    haData.grid_connected ? "connected" : "islanded",
                    haData.island_mode.c_str());
      return;
    }
  }
  
  // Fallback scanning removed – recv.text path is sufficient
} 