#pragma once

#include <GL/gl.h>

#include <string>
#include <vector>

#include "assets/bsp.h"
#include "assets/mdl.h"

// Shared fixed-function GL drawing used by the engine and the standalone
// verification tools, so map/model rendering behaves identically in all of them.

// Uploads an RGBA8 texture. A texture with no pixel data (an unresolved
// external WAD reference, say) becomes a magenta/black checker so gaps are
// obvious rather than invisible.
GLuint uploadTexture(const BspTexture& tex);
GLuint uploadTexture(const MdlTexture& tex);

std::vector<GLuint> uploadTextures(const std::vector<BspTexture>& textures);
std::vector<GLuint> uploadTextures(const std::vector<MdlTexture>& textures);

// Draws world geometry / a studio model with `texIds` from uploadTextures().
void drawBspFaces(const BspMap& map, const std::vector<GLuint>& texIds);
void drawMdlTriangles(const MdlModel& model, const std::vector<GLuint>& texIds);

// Reads the current framebuffer and writes it as a BMP, flipping GL's
// bottom-up rows to top-down. Returns false on failure.
bool saveScreenshotBMP(const std::string& path, int width, int height);
