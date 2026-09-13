#include "ui.h"
#include "font5x7.h"

#include <GL/gl.h>
#include <algorithm>

namespace {
int g_mouseX = 0, g_mouseY = 0;
bool g_mouseDown = false;
bool g_mouseWasDown = false;
int g_screenW = 1, g_screenH = 1;
const void* g_activeSlider = nullptr; // identity of the float* currently being dragged, if any
} // namespace

void uiBeginFrame(int mouseX, int mouseY, bool mouseDown, int screenW, int screenH) {
    g_mouseX = mouseX;
    g_mouseY = mouseY;
    g_mouseDown = mouseDown;
    g_screenW = screenW;
    g_screenH = screenH;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screenW, screenH, 0, -1, 1); // y-down, matches mouse coords
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}

void uiEndFrame() {
    g_mouseWasDown = g_mouseDown;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
}

void uiDrawRect(float x, float y, float w, float h, Color c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
    glColor4f(1, 1, 1, 1);
}

void uiDrawRectOutline(float x, float y, float w, float h, Color c, float thickness) {
    uiDrawRect(x, y, w, thickness, c);
    uiDrawRect(x, y + h - thickness, w, thickness, c);
    uiDrawRect(x, y, thickness, h, c);
    uiDrawRect(x + w - thickness, y, thickness, h, c);
}

static float drawChar(float x, float y, char ch, Color c, float scale) {
    const Glyph5x7* g = findGlyph(ch >= 'a' && ch <= 'z' ? ch - 32 : ch); // uppercase-only font
    if (!g) return 6.0f * scale;

    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (!(g->rows[row] & (1 << (4 - col)))) continue;
            float px = x + col * scale;
            float py = y + row * scale;
            glVertex2f(px, py);
            glVertex2f(px + scale, py);
            glVertex2f(px + scale, py + scale);
            glVertex2f(px, py + scale);
        }
    }
    glEnd();
    glColor4f(1, 1, 1, 1);
    return 6.0f * scale; // 5 wide + 1 spacing
}

float uiDrawText(float x, float y, const std::string& text, Color c, float scale) {
    float cursor = x;
    for (char ch : text) cursor += drawChar(cursor, y, ch, c, scale);
    return cursor - x;
}

float uiTextWidth(const std::string& text, float scale) {
    return (float)text.size() * 6.0f * scale;
}

bool uiMouseInRect(float x, float y, float w, float h) {
    return g_mouseX >= x && g_mouseX < x + w && g_mouseY >= y && g_mouseY < y + h;
}

bool uiButton(float x, float y, float w, float h, const std::string& label, Color bg, Color fg) {
    bool hovered = uiMouseInRect(x, y, w, h);
    Color drawColor = bg;
    if (hovered) {
        drawColor.r = std::min(1.0f, bg.r + 0.15f);
        drawColor.g = std::min(1.0f, bg.g + 0.15f);
        drawColor.b = std::min(1.0f, bg.b + 0.15f);
    }
    uiDrawRect(x, y, w, h, drawColor);
    uiDrawRectOutline(x, y, w, h, Color{0, 0, 0, 0.5f});

    float textW = uiTextWidth(label);
    uiDrawText(x + (w - textW) / 2.0f, y + (h - 7.0f * 2.0f) / 2.0f, label, fg);

    return hovered && g_mouseDown && !g_mouseWasDown;
}

bool uiSlider(float x, float y, float w, float h, float* value, float minV, float maxV, const std::string& label) {
    bool hovered = uiMouseInRect(x, y, w, h);
    if (g_mouseDown && !g_mouseWasDown && hovered) g_activeSlider = value;
    if (!g_mouseDown && g_activeSlider == value) g_activeSlider = nullptr;

    bool changed = false;
    if (g_activeSlider == value) {
        float t = std::clamp((g_mouseX - x) / w, 0.0f, 1.0f);
        float newValue = minV + t * (maxV - minV);
        if (newValue != *value) changed = true;
        *value = newValue;
    }

    if (!label.empty()) uiDrawText(x, y - 16, label, kColorWhite, 1.2f);
    uiDrawRect(x, y, w, h, Color{0.15f, 0.15f, 0.18f, 1.0f});
    float fillT = std::clamp((*value - minV) / (maxV - minV), 0.0f, 1.0f);
    uiDrawRect(x, y, w * fillT, h, Color{0.8f, 0.5f, 0.15f, 1.0f});
    uiDrawRectOutline(x, y, w, h, Color{0, 0, 0, 0.6f});
    // Handle.
    uiDrawRect(x + w * fillT - 3, y - 3, 6, h + 6, kColorWhite);

    return changed;
}

bool uiToggle(float x, float y, float w, float h, bool* value, const std::string& labelOn, const std::string& labelOff) {
    Color bg = *value ? Color{0.15f, 0.55f, 0.2f, 1.0f} : Color{0.4f, 0.15f, 0.15f, 1.0f};
    if (uiButton(x, y, w, h, *value ? labelOn : labelOff, bg)) {
        *value = !*value;
        return true;
    }
    return false;
}
