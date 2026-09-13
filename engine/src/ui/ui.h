#pragma once

#include <string>

// Minimal immediate-mode 2D UI toolkit for menu screens. No textures, no
// external font — text is drawn as quads from the built-in 5x7 bitmap font.
// Call uiBeginFrame() once per frame before any drawing/hit-testing, then
// uiEndFrame() after SDL_GL_SwapWindow (it advances the "was down" state).

struct Color { float r, g, b, a = 1.0f; };

constexpr Color kColorWhite{1, 1, 1, 1};
constexpr Color kColorBlack{0, 0, 0, 1};

void uiBeginFrame(int mouseX, int mouseY, bool mouseDown, int screenW, int screenH);
void uiEndFrame();

void uiDrawRect(float x, float y, float w, float h, Color c);
void uiDrawRectOutline(float x, float y, float w, float h, Color c, float thickness = 2.0f);

// scale=1 renders each font pixel as a 2x2 screen quad (5x7 font would be
// unreadably small otherwise). Returns the rendered width in pixels.
float uiDrawText(float x, float y, const std::string& text, Color c, float scale = 2.0f);
float uiTextWidth(const std::string& text, float scale = 2.0f);

// Returns true on the frame the button was clicked (mouse down transitioned
// inside the rect). Always draws the button (with hover highlight).
bool uiButton(float x, float y, float w, float h, const std::string& label, Color bg, Color fg = kColorWhite);

bool uiMouseInRect(float x, float y, float w, float h);

// Draws a horizontal slider (track + filled portion + label above it) and,
// while it's being dragged, writes the new value into *value (clamped to
// [minV, maxV]) and returns true. Tracks the active drag by the identity of
// `value` so dragging keeps working even if the mouse strays slightly
// outside the track rect.
bool uiSlider(float x, float y, float w, float h, float* value, float minV, float maxV, const std::string& label);

// A button that flips *value on click and shows labelOn/labelOff depending
// on its current state; returns true on the frame it was toggled.
bool uiToggle(float x, float y, float w, float h, bool* value, const std::string& labelOn, const std::string& labelOff);
