#include "particles.h"

#include <algorithm>

namespace {

GLuint uploadFrame(const SprFrame& frame) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.rgba.data());
    return id;
}

} // namespace

int ParticleSystem::loadEffect(const std::string& path) {
    Effect effect;
    if (!effect.sprite.load(path) || effect.sprite.frames().empty()) return -1;
    effect.frameTex.reserve(effect.sprite.frames().size());
    for (const auto& frame : effect.sprite.frames()) effect.frameTex.push_back(uploadFrame(frame));
    effects_.push_back(std::move(effect));
    return (int)effects_.size() - 1;
}

void ParticleSystem::spawn(int effect, Vec3f pos, float scale, float lifetime) {
    if (effect < 0 || (size_t)effect >= effects_.size()) return;
    particles_.push_back(Particle{effect, pos, 0.0f, lifetime, scale});
}

void ParticleSystem::update(float dt) {
    for (auto& p : particles_) p.age += dt;
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                     [](const Particle& p) { return p.age >= p.lifetime; }),
                      particles_.end());
}

void ParticleSystem::draw(Vec3f camRight, Vec3f camUp) const {
    if (particles_.empty()) return;

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE); // never occlude each other or write holes in the world's depth
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    for (const auto& p : particles_) {
        const Effect& effect = effects_[(size_t)p.effect];
        const auto& frames = effect.sprite.frames();
        float t = std::min(p.age / p.lifetime, 0.999f);
        size_t frameIndex = (size_t)(t * frames.size());
        if (frameIndex >= frames.size()) frameIndex = frames.size() - 1;
        const SprFrame& frame = frames[frameIndex];

        SprRenderMode mode = effect.sprite.renderMode();
        if (mode == SprRenderMode::Additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        // Fade the whole particle out over its last quarter-life so it
        // doesn't pop out of existence when the animation ends.
        float fadeIn = std::min(t / 0.1f, 1.0f);
        float fadeOut = std::min((1.0f - t) / 0.25f, 1.0f);
        float alpha = std::min(fadeIn, fadeOut);

        float halfW = frame.width * 0.5f * p.scale;
        float halfH = frame.height * 0.5f * p.scale;

        glBindTexture(GL_TEXTURE_2D, effect.frameTex[frameIndex]);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 1); glVertex3f(p.pos.x - camRight.x * halfW + camUp.x * -halfH,
                                        p.pos.y - camRight.y * halfW + camUp.y * -halfH,
                                        p.pos.z - camRight.z * halfW + camUp.z * -halfH);
        glTexCoord2f(1, 1); glVertex3f(p.pos.x + camRight.x * halfW + camUp.x * -halfH,
                                        p.pos.y + camRight.y * halfW + camUp.y * -halfH,
                                        p.pos.z + camRight.z * halfW + camUp.z * -halfH);
        glTexCoord2f(1, 0); glVertex3f(p.pos.x + camRight.x * halfW + camUp.x * halfH,
                                        p.pos.y + camRight.y * halfW + camUp.y * halfH,
                                        p.pos.z + camRight.z * halfW + camUp.z * halfH);
        glTexCoord2f(0, 0); glVertex3f(p.pos.x - camRight.x * halfW + camUp.x * halfH,
                                        p.pos.y - camRight.y * halfW + camUp.y * halfH,
                                        p.pos.z - camRight.z * halfW + camUp.z * halfH);
        glEnd();
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

ParticleSystem::~ParticleSystem() {
    for (auto& effect : effects_) {
        for (GLuint t : effect.frameTex) glDeleteTextures(1, &t);
    }
}
