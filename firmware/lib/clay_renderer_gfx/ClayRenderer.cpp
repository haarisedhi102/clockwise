#include "ClayRenderer.h"

#include <Arduino.h>
#include <math.h>

// Include Clay implementation in exactly one compilation unit
#define CLAY_IMPLEMENTATION
#include "../../third_party/clay/clay.h"

namespace ClayGfx {

static uint16_t g_w = 64;
static uint16_t g_h = 64;
static const GFXfont* g_font = nullptr; // single global font for measure+render

// Simple heap arena for Clay
static void* g_clayMem = nullptr;
static size_t g_clayMemSize = 0;

// Measure text using Adafruit_GFX bounds with the single default font
static Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
  (void)userData;
  Clay_Dimensions dim = {0, 0};
  if (!g_font || text.length <= 0 || !text.chars) return dim;

  // Use a tiny offscreen canvas solely for measuring
  static GFXcanvas16 measureCanvas(64, 64);
  measureCanvas.setFont(g_font);
  int16_t x1, y1; uint16_t w, h;
  // getTextBounds expects a C string; text is not null-terminated. Copy into a small temp buffer.
  // Limit copy to a safe size for our tiny UI strings.
  const int MAX_TMP = 96;
  char buf[MAX_TMP+1];
  int n = (text.length > MAX_TMP) ? MAX_TMP : text.length;
  memcpy(buf, text.chars, n);
  buf[n] = '\0';
  measureCanvas.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  // Account for letterSpacing crudely: add spacing between glyphs
  if (config && config->letterSpacing > 0 && n > 1) {
    w += (uint16_t)((n - 1) * config->letterSpacing);
  }
  dim.width = (float)w;
  dim.height = (float)h;
  return dim;
}

static inline uint16_t rgb565(Adafruit_GFX& tgt, const Clay_Color& c) {
  uint8_t r = (uint8_t)(c.r);
  uint8_t g = (uint8_t)(c.g);
  uint8_t b = (uint8_t)(c.b);
  return tgt.color565(r, g, b);
}

void init(uint16_t width, uint16_t height) {
  g_w = width; g_h = height;

  // Allocate Clay arena and initialize
  uint32_t need = Clay_MinMemorySize();
  g_clayMemSize = need;
  g_clayMem = malloc(g_clayMemSize);
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(g_clayMemSize, g_clayMem);
  (void)Clay_Initialize(arena, (Clay_Dimensions){(float)g_w, (float)g_h}, (Clay_ErrorHandler){nullptr, nullptr});

  // Register simple measure function
  Clay_SetMeasureTextFunction(MeasureText, nullptr);
}

void setDefaultFont(const GFXfont* font) {
  g_font = font;
}

// Naive rounded-rect fill (double loop) for tiny 64x64 canvas
static void fillRoundRect(Adafruit_GFX& t, int16_t x, int16_t y, int16_t w, int16_t h,
                          const Clay_CornerRadius& cr, uint16_t color) {
  // Clamp radii to half-dimensions
  int16_t rtl = (int16_t)min<float>(cr.topLeft, min(w/2, h/2));
  int16_t rtr = (int16_t)min<float>(cr.topRight, min(w/2, h/2));
  int16_t rbl = (int16_t)min<float>(cr.bottomLeft, min(w/2, h/2));
  int16_t rbr = (int16_t)min<float>(cr.bottomRight, min(w/2, h/2));

  // Simple scanline fill respecting corner circles
  for (int16_t yy = 0; yy < h; ++yy) {
    int16_t xStart = 0;
    int16_t xEnd = w - 1;
    // Top-left corner
    if (yy < rtl) {
      int16_t dy = rtl - yy;
      int16_t dx = (int16_t)roundf(sqrtf((float)rtl*rtl - (float)dy*dy));
      xStart = max<int16_t>(xStart, rtl - dx);
    }
    // Top-right corner
    if (yy < rtr) {
      int16_t dy = rtr - yy;
      int16_t dx = (int16_t)roundf(sqrtf((float)rtr*rtr - (float)dy*dy));
      xEnd = min<int16_t>(xEnd, w - 1 - (rtr - dx));
    }
    // Bottom-left corner
    if (yy >= h - rbl) {
      int16_t dy = yy - (h - rbl - 1);
      int16_t dx = (int16_t)roundf(sqrtf((float)rbl*rbl - (float)dy*dy));
      xStart = max<int16_t>(xStart, rbl - dx);
    }
    // Bottom-right corner
    if (yy >= h - rbr) {
      int16_t dy = yy - (h - rbr - 1);
      int16_t dx = (int16_t)roundf(sqrtf((float)rbr*rbr - (float)dy*dy));
      xEnd = min<int16_t>(xEnd, w - 1 - (rbr - dx));
    }
    if (xEnd >= xStart) t.drawFastHLine(x + xStart, y + yy, xEnd - xStart + 1, color);
  }
}

void render(Clay_RenderCommandArray renderCommands, Adafruit_GFX& target) {
  // Clear handled by caller (backbuffer.fillScreen)
  for (int32_t j = 0; j < renderCommands.length; ++j) {
    Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(&renderCommands, j);
    const int16_t bbX = (int16_t)cmd->boundingBox.x;
    const int16_t bbY = (int16_t)cmd->boundingBox.y;
    const int16_t bbW = (int16_t)cmd->boundingBox.width;
    const int16_t bbH = (int16_t)cmd->boundingBox.height;

    switch (cmd->commandType) {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        uint16_t c = rgb565(target, cmd->renderData.rectangle.backgroundColor);
        const auto& cr = cmd->renderData.rectangle.cornerRadius;
        if ((cr.topLeft + cr.topRight + cr.bottomLeft + cr.bottomRight) > 0.0f)
          fillRoundRect(target, bbX, bbY, bbW, bbH, cr, c);
        else
          target.fillRect(bbX, bbY, bbW, bbH, c);
        break;
      }
      case CLAY_RENDER_COMMAND_TYPE_BORDER: {
        uint16_t c = rgb565(target, cmd->renderData.border.color);
        const auto& w = cmd->renderData.border.width;
        if (w.top    > 0) target.fillRect(bbX, bbY, bbW, (int16_t)w.top, c);
        if (w.bottom > 0) target.fillRect(bbX, bbY + bbH - (int16_t)w.bottom, bbW, (int16_t)w.bottom, c);
        if (w.left   > 0) target.fillRect(bbX, bbY, (int16_t)w.left, bbH, c);
        if (w.right  > 0) target.fillRect(bbX + bbW - (int16_t)w.right, bbY, (int16_t)w.right, bbH, c);
        // betweenChildren not handled here (Clay already emits separate rects in other renderers)
        break;
      }
      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        if (!g_font) break;
        target.setFont(g_font);
        uint16_t col = rgb565(target, cmd->renderData.text.textColor);
        target.setTextColor(col);
        // Extract string slice into temp buffer
        const int MAX_TMP = 96;
        char buf[MAX_TMP+1];
        int n = (cmd->renderData.text.stringContents.length > MAX_TMP)
                  ? MAX_TMP : cmd->renderData.text.stringContents.length;
        memcpy(buf, cmd->renderData.text.stringContents.chars, n);
        buf[n] = '\0';
        // Compute baseline so that text fits within bounding box
        int16_t x1, y1; uint16_t w, h;
        target.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        int16_t baselineY = bbY - y1; // move so top of bounds aligns to bbY
        target.setCursor(bbX, baselineY);
        target.print(buf);
        break;
      }
      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      case CLAY_RENDER_COMMAND_TYPE_IMAGE:
      case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
      case CLAY_RENDER_COMMAND_TYPE_NONE:
      default:
        // Not implemented; skip
        break;
    }
  }
}

} // namespace ClayGfx
