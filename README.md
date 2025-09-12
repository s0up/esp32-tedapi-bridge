### Powerwall Charge on Solar (ESP32)

ESP32 firmware that connects directly to a Tesla Powerwall 3 gateway over its local Wi‑Fi and broadcasts key stats via BTHome BLE for easy Home Assistant discovery. Built for LilyGO TTGO T‑Display (ESP32), but easily portable to other ESP32 boards.

## Hardware
- **Target board**: LilyGO TTGO T‑Display (ESP32)
- **Display**: Onboard ST7789 240x135 TFT (rendered battery and power stats)
- **BLE**: NimBLE (BTHome v2, unencrypted)

## Setup
1) **Install tooling**
   - VS Code + PlatformIO extension

2) **Configure gateway Wi‑Fi credentials**
   - Rename `src/config.h.template` to `src/config.h`
   - Edit the file and set your Powerwall 3 gateway Wi‑Fi SSID and password:
     - `POWERWALL_WIFI_SSID` → typically looks like `TeslaPW_XXXXXX`
     - `POWERWALL_WIFI_PASSWORD` → printed on the gateway/inside the door
   - Note: `src/config.h` is already ignored by git.

3) **Build and upload**
   - Board: `env:lilygo-t-display` in `platformio.ini`
   - Click “Upload” in PlatformIO, or run `PlatformIO: Upload`
   - Open Serial Monitor at 115200 to view logs

## What it does
- Connects to the Powerwall gateway Wi‑Fi and queries TEDAPI locally (no cloud).
- Renders a simple battery/power view on the TFT display.
- Advertises a BTHome v2 BLE payload named `PW BTHome` for passive discovery.

## BTHome exposure
- **Protocol**: BTHome v2, unencrypted, service UUID `0xFCD2`
- **Advertised entities** (Home Assistant will discover these):
  - Battery percent (0–100)
  - Solar power (W)
  - Load power (W)
  - Site power (W) — grid import (+) / export (−) as reported
  - Grid connected (boolean)
- To fit BLE limits, power metrics alternate across frames; HA aggregates them automatically when within range.

## Networking notes
- The ESP32 must be in range of the Powerwall gateway’s Wi‑Fi. It connects only to that SSID and does not require internet.

## Porting to other ESP32 boards
- Update `platformio.ini` pins/display settings as needed, or stub out `display.*` if running headless.
- BLE and Powerwall logic are board‑agnostic; the main changes are GPIO/display config.

## Troubleshooting
- Not connecting: double‑check SSID/password in `src/config.h`, and ensure you’re near the gateway.
- No BLE in HA: enable the BTHome integration and ensure your HA host supports passive BLE; bring the ESP32 closer.
