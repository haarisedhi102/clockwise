#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Adafruit_GFX.h>

// Clockwise commons (WiFi/time if you want them later)
#include <CWPreferences.h>

// Clay adapter
#include "clay_renderer_gfx/ClayRenderer.h"
// Clay API for building layouts (no implementation here)
#include "../third_party/clay/clay.h"

// Fonts (choose a single default)
#include "../lib/cw-cf-0x02/small4pt7b.h"

#define ESP32_LED_BUILTIN 2

static MatrixPanel_I2S_DMA *g_panel = nullptr;
static GFXcanvas16 *g_canvas = nullptr; // backbuffer

static void setupPanel(bool swapBlueGreen, uint8_t displayBright, uint8_t displayRotation) {
  HUB75_I2S_CFG mxconfig(64, 64, 1);
  if (swapBlueGreen) {
    mxconfig.gpio.b1 = 26; mxconfig.gpio.b2 = 12;
    mxconfig.gpio.g1 = 27; mxconfig.gpio.g2 = 13;
  }
  mxconfig.gpio.e = 18;
  mxconfig.clkphase = false;

  g_panel = new MatrixPanel_I2S_DMA(mxconfig);
  g_panel->begin();
  g_panel->setBrightness8(displayBright);
  g_panel->clearScreen();
  g_panel->setRotation(displayRotation);

  g_canvas = new GFXcanvas16(64, 64);
  g_canvas->fillScreen(0x0000);
}

void setup() {
  Serial.begin(115200);
  pinMode(ESP32_LED_BUILTIN, OUTPUT);

  ClockwiseParams::getInstance()->load();

  setupPanel(ClockwiseParams::getInstance()->swapBlueGreen,
             ClockwiseParams::getInstance()->displayBright,
             ClockwiseParams::getInstance()->displayRotation);

  // Initialize Clay (requires third_party/clay/clay.h present)
  ClayGfx::init((uint16_t)64, (uint16_t)64);
  ClayGfx::setDefaultFont(&small4pt7b);
}

void loop() {
  // Build UI and render it fully each tick
  g_canvas->fillScreen(0x0000);

  // Minimal layout mirroring clay-rpi-matrix flow
  ClayGfx::beginLayout();
  CLAY(CLAY_ID("Root"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } }, .backgroundColor = {0,0,0,255} }) {
    CLAY_AUTO_ID({ .layout = { .padding = {4,4,4,4} }, .backgroundColor = {25,25,75,255}, .cornerRadius = CLAY_CORNER_RADIUS(4) }) {
      CLAY_TEXT(CLAY_STRING("Hello Clay"), CLAY_TEXT_CONFIG({ .textColor = {255,0,0,255}, .wrapMode = CLAY_TEXT_WRAP_NONE }));
    }
  }
  Clay_RenderCommandArray cmds = ClayGfx::endLayout();

  ClayGfx::render(cmds, *g_canvas);

  // Present in one blit
  g_panel->drawRGBBitmap(0, 0, g_canvas->getBuffer(), 64, 64);
}
