#include "display.h"

void Display::begin() {
  tft.init();
  // Landscape to match 240x135
  tft.setRotation(1);
  tft.fillScreen(bgColor);
  tft.setTextColor(fgColor, bgColor);
  tft.setTextSize(2);
  screenW = tft.width();
  screenH = tft.height();
  updateLayout();
}

void Display::showBoot() {
  tft.fillScreen(bgColor);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Powerwall", screenW / 2, screenH / 2 - (tft.fontHeight() / 2 + 2));
  tft.setTextSize(1);
  tft.drawString("Starting...", screenW / 2, screenH / 2 + (tft.fontHeight() / 2 + 4));
  tft.setTextSize(2);
}

void Display::render(const PowerwallData& data, const HomeAutomationData& ha, bool isConnected) {
  tft.fillScreen(bgColor);

  drawHeader(isConnected);
  int16_t y = headerH + padding;
  y = drawBattery(data, y) + padding;
  y = drawHA(ha, y) + padding;
}

void Display::drawHeader(bool isConnected) {
  int16_t h = headerH;
  tft.fillRect(0, 0, screenW, h, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(headerTextSize);
  int16_t hH = measureTextHeight(headerTextSize);
  int16_t yTop = (h - hH) / 2; if (yTop < 1) yTop = 1;
  tft.drawString("Powerwall", padding, yTop);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(isConnected ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
  tft.setTextSize(statusTextSize);
  tft.drawString(isConnected ? "Connected" : "Offline", screenW - padding, yTop);
}

void Display::drawBatteryBar(float percent, int16_t x, int16_t y, int16_t w, int16_t h) {
  if (percent < 0) percent = 0; if (percent > 100) percent = 100;
  uint16_t frame = TFT_WHITE;
  uint16_t fill = percent > 80 ? TFT_GREEN : (percent > 30 ? TFT_YELLOW : TFT_RED);
  // Draw battery outline
  tft.drawRect(x, y, w, h, frame);
  // battery tip
  int16_t tipW = 6;
  int16_t tipH = h / 3;
  int16_t tipY = y + (h - tipH) / 2;
  tft.drawRect(x + w, tipY, tipW, tipH, frame);
  // Fill level
  int16_t inner = w - 4;
  int16_t level = (int16_t)(inner * (percent / 100.0f));
  tft.fillRect(x + 2, y + 2, level, h - 4, fill);
}

int16_t Display::drawBattery(const PowerwallData& data, int16_t startY) {
  int16_t left = padding;
  float pct = data.data_valid ? data.battery_level : 0.0f;
  drawBatteryBar(pct, barX, startY, barW, barH);

  // Percent text sized to fit percentAreaW
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fgColor, bgColor);
  char line[32];
  if (data.data_valid) snprintf(line, sizeof(line), "%.1f%%", pct); else snprintf(line, sizeof(line), "--.-%%");
  int fitSize = fitTextSizeForBox(line, percentAreaW - percentGap, barH);
  tft.setTextSize(fitSize);
  tft.drawString(line, barX + barW + padding + percentGap, startY + (barH - measureTextHeight(fitSize)) / 2);
  tft.setTextSize(haTextSize);

  // Energy line below bar
  char ebuf[48];
  if (data.data_valid && data.total_pack_energy > 0) {
    snprintf(ebuf, sizeof(ebuf), "Rem %.0f / %.0f Wh", data.energy_remaining, data.total_pack_energy);
  } else {
    snprintf(ebuf, sizeof(ebuf), "Rem -- / -- Wh");
  }
  tft.setTextDatum(TL_DATUM);
  int eSize = fitTextSizeForBox(ebuf, screenW - 2 * padding, textH);
  tft.setTextSize(eSize);
  tft.drawString(ebuf, left, startY + barH + lineGap + 2);
  int16_t usedH = measureTextHeight(eSize);
  tft.setTextSize(haTextSize);
  return startY + barH + lineGap + 2 + usedH;
}

int16_t Display::drawHA(const HomeAutomationData& ha, int16_t startY) {
  int16_t y = startY + lineGap;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(accentColor, bgColor);
  // Keep the section title modest to avoid over-scaling
  int tSize = min(2, fitTextSizeForBox("Power (W)", screenW - 2 * padding, textH));
  tft.setTextSize(tSize);
  tft.drawString("Power (W)", padding, y);
  y += measureTextHeight(tSize) + lineGap;
  tft.setTextColor(fgColor, bgColor);

  char buf[64];
  if (ha.valid) {
    snprintf(buf, sizeof(buf), "Site: %.0f  Load: %.0f", ha.site_power_w, ha.load_power_w);
    int s1 = fitTextSizeForBox(buf, screenW - 2 * padding, textH);
    tft.setTextSize(s1);
    tft.drawString(buf, padding, y); y += measureTextHeight(s1) + lineGap;
    snprintf(buf, sizeof(buf), "Solar: %.0f  Batt: %.0f", ha.solar_power_w, ha.battery_power_w);
    int s2 = fitTextSizeForBox(buf, screenW - 2 * padding, textH);
    tft.setTextSize(s2);
    tft.drawString(buf, padding, y); y += measureTextHeight(s2) + lineGap;
    snprintf(buf, sizeof(buf), "Grid: %s  Mode: %s", ha.grid_connected ? "Yes" : "No", ha.island_mode.c_str());
    int s3 = fitTextSizeForBox(buf, screenW - 2 * padding, textH);
    tft.setTextSize(s3);
    tft.drawString(buf, padding, y); y += measureTextHeight(s3) + lineGap;
  } else {
    int s0 = fitTextSizeForBox("No HA data", screenW - 2 * padding, textH);
    tft.setTextSize(s0);
    tft.drawString("No HA data", padding, y); y += measureTextHeight(s0) + lineGap;
  }
  return y;
}

void Display::updateLayout() {
  // Metrics using helpers to avoid clipping
  textH = measureTextHeight(haTextSize);
  headerH = measureTextHeight(headerTextSize) + 2 * padding;
  // Battery bar height: ensure room for HA section; allocate ~35% of remaining height
  int16_t remainingH = screenH - headerH - 4 * padding - measureTextHeight(haTextSize) * 4; // HA title + 3 lines
  if (remainingH < 40) remainingH = 40;
  barH = (int16_t)max((int16_t)18, (int16_t)(remainingH * 6 / 10));
  // Layout battery bar and percent side-by-side
  percentAreaW = 72; // Slightly smaller to keep gap from bar
  barX = padding;
  barW = screenW - (barX + percentAreaW + 3 * padding + 6); // tip+padding
  if (barW < 90) { barW = 90; percentAreaW = screenW - barX - barW - 3 * padding - 6; if (percentAreaW < 48) percentAreaW = 48; }
}

int16_t Display::measureTextWidth(const char* s, int size) {
  tft.setTextSize(size);
  return tft.textWidth(s);
}

int16_t Display::measureTextHeight(int size) {
  tft.setTextSize(size);
  return tft.fontHeight();
}

int Display::fitTextSizeForBox(const char* s, int maxW, int maxH) {
  int size = min(percentTextMaxSize, 4); // guard upper bound
  for (; size >= 1; size--) {
    int w = measureTextWidth(s, size);
    int h = measureTextHeight(size);
    if (w <= maxW && h <= maxH) return size;
  }
  return 1;
}


