#pragma once

#include <string>
#include <vector>

#include "glext.h"
#include "../mat4.h"
#include "../assets/spr.h"

// World-space sprite particle system: muzzle flashes, bullet-impact smoke,
// explosions — anything that's a short-lived billboarded .spr animation
// planted at a point in the world. Not a general particle simulator (no
// velocity/gravity) since nothing this engine spawns yet needs to move.
class ParticleSystem {
public:
    // Loads a .spr and uploads each of its frames as a texture, returning a
    // handle to pass to spawn(). Returns -1 (spawn() becomes a silent no-op
    // for that handle) if the file can't be loaded, so a missing effect
    // sprite degrades gracefully instead of crashing the engine.
    int loadEffect(const std::string& path);

    // Spawns one instance of an already-loaded effect at a world position.
    // scale is world units per sprite pixel; lifetime is seconds, spread
    // evenly across the sprite's frames (so a 3-frame flash and a 30-frame
    // explosion both just play their whole animation once and die).
    void spawn(int effect, Vec3f pos, float scale, float lifetime);

    void update(float dt);

    // Draws all live particles as camera-facing quads. Expects the caller's
    // current GL_PROJECTION/GL_MODELVIEW to already be the world proj/view
    // (same convention as the bullet impact marks in main.cpp); camRight/
    // camUp are the camera's world-space right/up axes, used to billboard
    // each quad toward the viewer.
    void draw(Vec3f camRight, Vec3f camUp) const;

    ~ParticleSystem();

private:
    struct Effect {
        SprModel sprite;
        std::vector<GLuint> frameTex;
    };
    struct Particle {
        int effect;
        Vec3f pos;
        float age = 0.0f;
        float lifetime = 1.0f;
        float scale = 1.0f;
    };

    std::vector<Effect> effects_;
    std::vector<Particle> particles_;
};
