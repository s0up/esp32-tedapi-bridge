#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "powerwall.h"

class Display {
private:
  TFT_eSPI tft;
  uint16_t bgColor = TFT_BLACK;
  uint16_t fgColor = TFT_WHITE;
  uint16_t accentColor = TFT_CYAN;
  int16_t screenW = 240; // after rotation
  int16_t screenH = 135; // after rotation
  int16_t padding = 4;
  int16_t lineGap = 2;
  int16_t textH = 0;
  int16_t headerH = 0;
  int16_t barH = 0;
  int16_t barW = 0;
  int16_t barX = 0;
  int16_t percentAreaW = 0;
  int16_t percentGap = 8;
  int headerTextSize = 2;
  int statusTextSize = 1;
  int haTextSize = 2;
  int percentTextMaxSize = 2;

  void updateLayout();
  int16_t measureTextWidth(const char* s, int size);
  int16_t measureTextHeight(int size);
  int fitTextSizeForBox(const char* s, int maxW, int maxH);

  void drawHeader(bool isConnected);
  int16_t drawBattery(const PowerwallData& data, int16_t startY);
  int16_t drawHA(const HomeAutomationData& ha, int16_t startY);
  void drawBatteryBar(float percent, int16_t x, int16_t y, int16_t w, int16_t h);

public:
  void begin();
  void showBoot();
  void render(const PowerwallData& data, const HomeAutomationData& ha, bool isConnected);
};

#endif // DISPLAY_H


