/*
*******************************************************************************
* Copyright (c) 2025 by serginator
*
* Cardputer Mouse Jiggler v1.0.5
*
* Describe: M5Stack Cardputer Mouse Jiggler
* Date: 2025/08/21
*******************************************************************************
*/

#include <M5Cardputer.h>
#include <USB.h>
#include <USBHIDMouse.h>

USBHIDMouse Mouse;

unsigned long lastMoveTime = 0;
unsigned long MIN_DELAY_MS = 240000;  // 4 minutes
unsigned long MAX_DELAY_MS = 300000; // 5 minutes
unsigned long currentDelay = MIN_DELAY_MS;
bool jiggling = true;
bool screen = true;

bool upPressed = false, downPressed = false, leftPressed = false, rightPressed = false, enterPressed = false, spacePressed = false;

unsigned long jiggleCount = 0;
uint8_t brightness = 0;

// --------------------------------------------
// Cyberpunk UI helpers and drawing primitives
// --------------------------------------------

// Neon palette
static const uint32_t COLOR_BG            = 0x05060A;  // near-black
static const uint32_t COLOR_GRID          = 0x0E1622;  // subtle grid
static const uint32_t COLOR_NEON_CYAN     = 0x00E5FF;
static const uint32_t COLOR_NEON_YELLOW   = 0xFFE900;
static const uint32_t COLOR_NEON_MAGENTA  = 0xFF2FBF;
static const uint32_t COLOR_TEXT_PRIMARY  = 0xCFE7FF;
static const uint32_t COLOR_TEXT_MUTED    = 0x7AA9C7;

// Layout constants
static const int16_t UI_HEADER_H = 20;  // height reserved for top title

// Utility: dim an RGB color by factor [0..1]
static uint32_t dimColor(uint32_t rgb, float factor) {
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = (rgb) & 0xFF;
  r = (uint8_t)(r * factor);
  g = (uint8_t)(g * factor);
  b = (uint8_t)(b * factor);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void drawGridBackground(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t step) {
  auto &d = M5Cardputer.Display;
  d.fillRect(x, y, w, h, COLOR_BG);
  for (int16_t gx = x; gx <= x + w; gx += step) {
    d.drawLine(gx, y, gx, y + h, COLOR_GRID);
  }
  for (int16_t gy = y; gy <= y + h; gy += step) {
    d.drawLine(x, gy, x + w, gy, COLOR_GRID);
  }
}

static void drawNeonPanel(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t mainColor, uint8_t r = 10) {
  auto &d = M5Cardputer.Display;
  // glow lines
  d.drawRoundRect(x - 2, y - 2, w + 4, h + 4, r + 2, dimColor(mainColor, 0.25f));
  d.drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, dimColor(mainColor, 0.5f));
  d.drawRoundRect(x, y, w, h, r, mainColor);
}

static void fillNeonButton(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t fillColor, uint32_t edgeColor, uint8_t r = 10) {
  auto &d = M5Cardputer.Display;
  d.fillRoundRect(x, y, w, h, r, dimColor(fillColor, 0.15f));
  d.drawRoundRect(x, y, w, h, r, edgeColor);
  d.drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, dimColor(edgeColor, 0.35f));
}

static void drawArrowUp(int16_t cx, int16_t cy, int16_t size, uint32_t color) {
  auto &d = M5Cardputer.Display;
  d.fillTriangle(cx, cy - size, cx - size, cy + size, cx + size, cy + size, color);
}
static void drawArrowDown(int16_t cx, int16_t cy, int16_t size, uint32_t color) {
  auto &d = M5Cardputer.Display;
  d.fillTriangle(cx, cy + size, cx - size, cy - size, cx + size, cy - size, color);
}
static void drawArrowLeft(int16_t cx, int16_t cy, int16_t size, uint32_t color) {
  auto &d = M5Cardputer.Display;
  d.fillTriangle(cx - size, cy, cx + size, cy - size, cx + size, cy + size, color);
}
static void drawArrowRight(int16_t cx, int16_t cy, int16_t size, uint32_t color) {
  auto &d = M5Cardputer.Display;
  d.fillTriangle(cx + size, cy, cx - size, cy - size, cx - size, cy + size, color);
}

static void drawOkButton(int16_t cx, int16_t cy, int16_t radius, uint32_t colorEdge, uint32_t colorFill) {
  auto &d = M5Cardputer.Display;
  d.fillCircle(cx, cy, radius, dimColor(colorFill, 0.2f));
  d.drawCircle(cx, cy, radius, colorEdge);
  d.drawCircle(cx, cy, radius + 1, dimColor(colorEdge, 0.35f));
  d.setTextColor(COLOR_TEXT_PRIMARY);
  d.setTextSize(1.4f);
  d.setTextDatum(middle_center);
  d.drawString("OK", cx + 1, cy);
  d.setTextDatum(top_left);
}

static void drawMouseIcon(int16_t x, int16_t y, float scale, uint32_t accent) {
  auto &d = M5Cardputer.Display;
  int16_t w = (int16_t)(50 * scale);
  int16_t h = (int16_t)(80 * scale);
  int16_t r = (int16_t)(18 * scale);
  // body
  fillNeonButton(x, y, w, h, accent, accent, r);
  // split line (buttons)
  d.drawLine(x, y + h / 2, x + w, y + h / 2, dimColor(accent, 0.4f));
  // wheel
  d.fillRoundRect(x + w / 2 - (int)(3 * scale), y + (int)(12 * scale), (int)(6 * scale), (int)(18 * scale), (int)(3 * scale), dimColor(accent, 0.85f));
  // tail
  d.drawBezier(x + w, y + h - (int)(10 * scale), x + w + (int)(12 * scale), y + h + (int)(2 * scale), x + w + (int)(28 * scale), y + h - (int)(6 * scale), x + w + (int)(40 * scale), y + h + (int)(4 * scale), dimColor(accent, 0.6f));
}

static void drawDpad(int16_t x, int16_t y, int16_t size) {
  // size is the half-length from center to button center
  auto &d = M5Cardputer.Display;
  int16_t btnW = 32, btnH = 32, radius = 8;
  int16_t arrowSize = 8;
  // Up (centered on x)
  fillNeonButton(x - btnW / 2, y - size - btnH / 2, btnW, btnH, COLOR_NEON_CYAN, COLOR_NEON_CYAN, radius);
  drawArrowUp(x, y - size, arrowSize, COLOR_NEON_CYAN);
  // Down (centered on x)
  fillNeonButton(x - btnW / 2, y + size - btnH / 2, btnW, btnH, COLOR_NEON_CYAN, COLOR_NEON_CYAN, radius);
  drawArrowDown(x, y + size, arrowSize, COLOR_NEON_CYAN);
  // Left
  fillNeonButton(x - size - btnW / 2, y - btnH / 2, btnW, btnH, COLOR_NEON_CYAN, COLOR_NEON_CYAN, radius);
  drawArrowLeft(x - size, y, arrowSize, COLOR_NEON_CYAN);
  // Right
  fillNeonButton(x + size - btnW / 2, y - btnH / 2, btnW, btnH, COLOR_NEON_CYAN, COLOR_NEON_CYAN, radius);
  drawArrowRight(x + size, y, arrowSize, COLOR_NEON_CYAN);
  // OK
  drawOkButton(x, y, 18, COLOR_NEON_YELLOW, COLOR_NEON_YELLOW);
}

static void drawInfoPanel(bool isJiggling, unsigned long nextDelayMs) {
  auto &d = M5Cardputer.Display;
  int16_t w = d.width();
  int16_t h = d.height();
  int16_t panelX = w / 2 + 6;
  int16_t panelY = UI_HEADER_H + 6;
  int16_t panelW = w - panelX - 8;
  int16_t panelH = h - panelY - 8;

  drawNeonPanel(panelX, panelY, panelW, panelH, COLOR_NEON_MAGENTA, 10);

  // Compact info: Status, Min, Max, Next, Batt
  d.setTextDatum(top_left);
  d.setTextSize(1.2f);
  int16_t lineY = panelY + 10;
  int16_t textX = panelX + 10;

  // Status
  d.setTextColor(COLOR_TEXT_MUTED);
  d.drawString("Status:", textX, lineY);
  d.setTextColor(isJiggling ? COLOR_NEON_YELLOW : COLOR_NEON_CYAN);
  d.drawString(isJiggling ? " RUN" : " IDLE", textX + 45, lineY);

  // Min
  lineY += 16;
  d.setTextColor(COLOR_TEXT_PRIMARY);
  d.drawString(String("Min: ") + String(MIN_DELAY_MS / 60000) + " min", textX, lineY);

  // Max
  lineY += 16;
  d.drawString(String("Max: ") + String(MAX_DELAY_MS / 60000) + " min", textX, lineY);

  // Next
  lineY += 16;
  if (isJiggling) {
    d.drawString(String("Next: ") + String(nextDelayMs / 1000) + " sec", textX, lineY);
  } else {
    d.drawString("Next: —", textX, lineY);
  }

  // Battery
  lineY += 16;
  d.setTextColor(COLOR_TEXT_MUTED);
  d.drawString(String("Batt: ") + String(M5Cardputer.Power.getBatteryLevel()) + "%", textX, lineY);
}

static void drawMainUI(bool isJiggling, unsigned long nextDelayMs) {
  auto &d = M5Cardputer.Display;
  int16_t w = d.width();
  int16_t h = d.height();

  // Background split
  drawGridBackground(0, 0, w / 2, h, 10);
  drawGridBackground(w / 2, 0, w - w / 2, h, 10);

  // Top title
  d.setTextDatum(top_center);
  d.setTextColor(COLOR_NEON_YELLOW);
  d.setTextSize(1.4f);
  d.drawString("Mouse Jiggler 1.0.5", w / 2, 2);

  // Left pane frame and D-Pad (reduced height below header)
  int16_t leftX = 6;
  int16_t leftY = UI_HEADER_H + 6;
  int16_t leftW = w / 2 - 12;
  int16_t leftH = h - leftY - 8;
  drawNeonPanel(leftX, leftY, leftW, leftH, COLOR_NEON_CYAN, 10);
  int16_t centerX = leftX + leftW / 2; // centered in left panel
  int16_t centerY = leftY + leftH / 2;
  drawDpad(centerX, centerY, 32);

  // Right info panel
  drawInfoPanel(isJiggling, nextDelayMs);
}

void updateDisplay() {
  drawMainUI(false, 0);
}

static void drawSplashScreen() {
  auto &d = M5Cardputer.Display;
  d.fillScreen(COLOR_BG);

  // Title
  d.setTextDatum(top_left);
  d.setTextSize(1.8f);
  d.setTextColor(COLOR_NEON_YELLOW);
  d.drawString("Cardputer Mouse Jiggler", 10, 10);
  d.setTextSize(1.2f);
  d.setTextColor(COLOR_TEXT_PRIMARY);
  d.drawString("v1.0.5", 10, 34);

  // Mouse graphic center-right
  drawMouseIcon(140, 30, 1.3f, COLOR_NEON_MAGENTA);

  // Author
  d.setTextColor(COLOR_TEXT_MUTED);
  d.drawString("by serginator", 10, 54);
}

void performMouseJiggle() {
  int moveX = random(30, 51);
  int moveY = random(-5, 6);

  Mouse.move(moveX, moveY);
  delay(200);
  Mouse.move(-moveX, -moveY);
}

void setup() {

    delay(3000);
    pinMode(0, INPUT_PULLUP);

    if (digitalRead(0) == LOW) {
        while (true) {
            delay(100);
        }
    }

  M5Cardputer.begin();
  USB.begin();
  Mouse.begin();

  Serial.begin(115200);
  Serial.println("Mouse Jiggler initialized");
  brightness = M5Cardputer.Display.getBrightness();

  drawSplashScreen();
  delay(2500);

  // Start in RUN mode
  currentDelay = random(MIN_DELAY_MS, MAX_DELAY_MS + 1);
  drawMainUI(true, currentDelay);
  performMouseJiggle();
}

void loop() {
  M5Cardputer.update();

  if (!jiggling) {
    if (M5Cardputer.Keyboard.isKeyPressed(';') && !upPressed) {  // Up button
      MIN_DELAY_MS = min(MAX_DELAY_MS - 60000, MIN_DELAY_MS + 60000);
      updateDisplay();
      upPressed = true;
    } else if (!M5Cardputer.Keyboard.isKeyPressed(';')) {
      upPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed('.') && !downPressed) {  // Down button
      MIN_DELAY_MS = (MIN_DELAY_MS >= 60000) ? MIN_DELAY_MS - 60000 : 0;
      updateDisplay();
      downPressed = true;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('.')) {
      downPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(',') && !leftPressed) {  // Left button
      MAX_DELAY_MS = max(MIN_DELAY_MS + 60000, MAX_DELAY_MS - 60000);
      updateDisplay();
      leftPressed = true;
    } else if (!M5Cardputer.Keyboard.isKeyPressed(',')) {
      leftPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed('/') && !rightPressed) {  // Right button
      MAX_DELAY_MS = min(3600000UL, MAX_DELAY_MS + 60000);
      updateDisplay();
      rightPressed = true;
    } else if (!M5Cardputer.Keyboard.isKeyPressed('/')) {
      rightPressed = false;
    }

    if (M5Cardputer.Keyboard.isKeyPressed(0x28) && !enterPressed) {  // ENTER button
      jiggling = true;
      currentDelay = random(MIN_DELAY_MS, MAX_DELAY_MS + 1);
      drawMainUI(true, currentDelay);
      enterPressed = true;
      performMouseJiggle();
    } else if (!M5Cardputer.Keyboard.isKeyPressed(0x28)) {
      enterPressed = false;
    }
  } else {
    unsigned long currentTime = millis();

    if (currentTime - lastMoveTime >= currentDelay) {
      performMouseJiggle();
      lastMoveTime = currentTime;

      currentDelay = random(MIN_DELAY_MS, MAX_DELAY_MS + 1);

      jiggleCount++;
      drawMainUI(true, currentDelay);
    }
    if (M5Cardputer.Keyboard.isKeyPressed(0x28) && !enterPressed) {  // ENTER button to stop
      jiggling = false;
      jiggleCount = 0;
      updateDisplay();
      enterPressed = true;
    } else if (!M5Cardputer.Keyboard.isKeyPressed(0x28)) {
      enterPressed = false;
    }
  }
  if (M5Cardputer.Keyboard.isKeyPressed(' ') && !spacePressed) {  // SPACE button to toggle display
    screen = !screen;
    if (screen) {
      M5Cardputer.Display.setBrightness(brightness);
    } else {
      M5Cardputer.Display.setBrightness(0);
    }
    spacePressed = true;
  } else if (!M5Cardputer.Keyboard.isKeyPressed(' ')) {
    spacePressed = false;
  }
}
