#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

// Forward declare clay types without including the implementation here
#include "../../third_party/clay/clay.h"

namespace ClayGfx {

// Initialize Clay with a simple arena and set the target dimensions
void init(uint16_t width, uint16_t height);

// Optionally set a single default GFX font to be used for both measuring and rendering
void setDefaultFont(const GFXfont* font);

// Convenience passthroughs for layout lifecycle
inline void beginLayout() { Clay_BeginLayout(); }
inline Clay_RenderCommandArray endLayout() { return Clay_EndLayout(); }

// Render Clay render commands to an Adafruit_GFX target (e.g., GFXcanvas16)
void render(Clay_RenderCommandArray renderCommands, Adafruit_GFX& target);

} // namespace ClayGfx
