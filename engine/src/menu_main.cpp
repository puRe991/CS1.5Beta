// CS:GO-inspired main menu: top nav bar (Play/Watch/Inventory/Store),
// a fictive-currency economy, and a functional Inventory + Store +
// Case-Opening loop. Play/Watch are placeholder screens (out of scope
// for this pass — see README TODO).
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <libgen.h>
#include <map>
#include <unistd.h>
#include <vector>

#include "ui/ui.h"
#include "inventory.h"
#include "assets/mdl.h"
#include "settings.h"

namespace {

enum class Screen { Play, Watch, Inventory, Store, Settings, CaseOpening };
enum class SettingsTab { Video, Audio, Controls, Mouse, Crosshair };

// Loads a weapon's base world-model texture once per weapon name and hands
// back a GL texture id tinted per skin, cached by skin name.
class SkinIconCache {
public:
    explicit SkinIconCache(std::string modelsDir) : modelsDir_(std::move(modelsDir)) {}

    GLuint get(const SkinDef& skin) {
        auto cached = iconCache_.find(skin.skinName);
        if (cached != iconCache_.end()) return cached->second;

        const MdlTexture* base = baseTexture(skin.weapon);
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (base && !base->rgba.empty()) {
            std::vector<uint8_t> tinted(base->rgba.size());
            for (size_t p = 0; p < base->rgba.size(); p += 4) {
                float mul[3] = {skin.tint[0], skin.tint[1], skin.tint[2]};
                for (int c = 0; c < 3; ++c) {
                    float v = base->rgba[p + c] * mul[c];
                    tinted[p + c] = (uint8_t)(v > 255.0f ? 255 : v);
                }
                tinted[p + 3] = base->rgba[p + 3];
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, base->width, base->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tinted.data());
        } else {
            uint8_t fallback[4] = {180, 180, 180, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
        }

        iconCache_[skin.skinName] = id;
        return id;
    }

private:
    const MdlTexture* baseTexture(const std::string& weapon) {
        auto it = weaponModels_.find(weapon);
        if (it == weaponModels_.end()) {
            MdlModel model;
            model.load(modelsDir_ + "/w_" + weapon + ".mdl");
            it = weaponModels_.emplace(weapon, std::move(model)).first;
        }
        const auto& textures = it->second.textures();
        return textures.empty() ? nullptr : &textures[0];
    }

    std::string modelsDir_;
    std::map<std::string, MdlModel> weaponModels_;
    std::map<std::string, GLuint> iconCache_;
};

void drawIconQuad(GLuint tex, float x, float y, float w, float h) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

std::string upper(std::string s) {
    for (char& c : s) if (c >= 'a' && c <= 'z') c -= 32;
    return s;
}

std::vector<std::string> listMaps(const std::string& mapsDir) {
    std::vector<std::string> maps;
    DIR* dir = opendir(mapsDir.c_str());
    if (!dir) return maps;
    while (dirent* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".bsp") {
            maps.push_back(name.substr(0, name.size() - 4));
        }
    }
    closedir(dir);
    std::sort(maps.begin(), maps.end());
    return maps;
}

// Directory the csmenu binary itself lives in, so we can find cs15engine
// sitting right next to it without requiring it on $PATH.
std::string exeDir(const char* argv0) {
    std::vector<char> buf(argv0, argv0 + std::strlen(argv0) + 1);
    return dirname(buf.data());
}

// Launches cs15engine as a detached child process for the chosen map.
void launchMap(const std::string& exeDirPath, const std::string& cstrikeDir, const std::string& mapName) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid(); // detach into its own session so it outlives the launching menu
        std::string enginePath = exeDirPath + "/cs15engine";
        std::string mapPath = cstrikeDir + "/maps/" + mapName + ".bsp";
        std::string viewModel = cstrikeDir + "/models/v_ak47.mdl";
        execl(enginePath.c_str(), enginePath.c_str(), mapPath.c_str(), cstrikeDir.c_str(), viewModel.c_str(), (char*)nullptr);
        std::fprintf(stderr, "launchMap: execl(%s) failed: %s\n", enginePath.c_str(), std::strerror(errno));
        std::_Exit(127); // execl only returns on failure
    }
    // Parent (the menu) keeps running; the child is left to the OS/init to reap.
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <cstrike_dir>\n", argv[0]);
        return 1;
    }
    std::srand((unsigned)std::time(nullptr));
    SettingsLoad();
    std::string cstrikeDir = argv[1];
    std::string modelsDir = cstrikeDir + "/models";
    std::string menuExeDir = exeDir(argv[0]);
    SkinIconCache icons(modelsDir);
    std::vector<std::string> maps = listMaps(cstrikeDir + "/maps");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    const int kWidth = 1280, kHeight = 720;
    SDL_Window* window = SDL_CreateWindow("cs15engine - Main Menu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           kWidth, kHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) { std::fprintf(stderr, "GL context failed: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::vector<CaseDef> cases = buildDefaultCases();
    PlayerEconomy econ;

    bool demoSeed = argc >= 5 && std::string(argv[4]) == "demo";
    if (demoSeed) {
        buyCase(econ, cases[0]);
        buyCase(econ, cases[0]);
        buyCase(econ, cases[1]);
        OwnedSkin dummy;
        openCase(econ, cases[0], dummy);
        openCase(econ, cases[1], dummy);
    }

    Screen screen = Screen::Inventory;
    SettingsTab settingsTab = SettingsTab::Video;
    Cvar* awaitingBind = nullptr; // non-null while the next keypress should be captured as a rebind
    std::string selectedMap = maps.empty() ? "" : maps[0];
    const CaseDef* openingCase = nullptr;
    OwnedSkin revealResult;
    bool revealDone = false;
    float spinTimer = 0.0f;
    constexpr float kSpinDuration = 1.2f;

    // Screenshot mode for automated verification, mirrors the other tools.
    std::string screenshotPath = argc >= 3 ? argv[2] : "";
    std::string screenshotScreen = argc >= 4 ? argv[3] : "";
    if (screenshotScreen == "store") screen = Screen::Store;
    if (screenshotScreen == "inventory") screen = Screen::Inventory;
    if (screenshotScreen == "play") screen = Screen::Play;
    if (screenshotScreen == "watch") screen = Screen::Watch;
    if (screenshotScreen == "settings") screen = Screen::Settings;
    if (screenshotScreen == "caseopen_spin" && !cases.empty()) {
        openingCase = &cases[0];
        screen = Screen::CaseOpening;
    }
    if (screenshotScreen == "caseopen_reveal" && !cases.empty()) {
        buyCase(econ, cases[0]);
        openingCase = &cases[0];
        openCase(econ, *openingCase, revealResult);
        revealDone = true;
        screen = Screen::CaseOpening;
    }

    Uint64 lastTicks = SDL_GetPerformanceCounter();
    bool running = true;

    while (running) {
        int mouseX, mouseY;
        Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);
        bool mouseDown = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (awaitingBind) {
                // Capturing the next physical key for a rebind: anything but
                // Escape (which cancels) is stored as this bind's new key.
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym != SDLK_ESCAPE) {
                        awaitingBind->SetString(SDL_GetScancodeName(event.key.keysym.scancode));
                    }
                    awaitingBind = nullptr;
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - lastTicks) / (float)SDL_GetPerformanceFrequency();
        lastTicks = now;

        glViewport(0, 0, kWidth, kHeight);
        glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        uiBeginFrame(mouseX, mouseY, mouseDown, kWidth, kHeight);

        // --- Top nav bar ---
        const Color kNavBg{0.12f, 0.13f, 0.16f, 1.0f};
        const Color kNavActive{0.85f, 0.55f, 0.15f, 1.0f}; // CS-orange accent
        uiDrawRect(0, 0, kWidth, 64, kNavBg);

        struct Tab { const char* label; Screen s; };
        Tab tabs[] = {{"PLAY", Screen::Play}, {"WATCH", Screen::Watch}, {"INVENTORY", Screen::Inventory}, {"STORE", Screen::Store}, {"SETTINGS", Screen::Settings}};
        float tabX = 24;
        for (auto& t : tabs) {
            float w = uiTextWidth(t.label) + 32;
            Color bg = (screen == t.s) ? kNavActive : kNavBg;
            if (uiButton(tabX, 8, w, 48, t.label, bg)) {
                screen = t.s;
            }
            tabX += w + 8;
        }

        char balanceStr[64];
        std::snprintf(balanceStr, sizeof(balanceStr), "$ %d COINS", econ.balance);
        float balW = uiTextWidth(balanceStr);
        uiDrawText(kWidth - balW - 24, 24, balanceStr, Color{0.4f, 1.0f, 0.4f, 1.0f});

        // --- Screens ---
        if (screen == Screen::Play) {
            uiDrawText(24, 84, "PLAY", kColorWhite, 3.0f);
            uiDrawText(24, 116, "SELECT A MAP, THEN GO - LAUNCHES CS15ENGINE", Color{0.6f, 0.6f, 0.6f, 1.0f}, 1.2f);

            if (maps.empty()) {
                uiDrawText(24, 160, "NO MAPS FOUND", Color{0.6f, 0.3f, 0.3f, 1.0f}, 1.5f);
            } else {
                float mx = 24, my = 150;
                int col = 0;
                for (auto& map : maps) {
                    bool selected = (map == selectedMap);
                    Color bg = selected ? Color{0.7f, 0.3f, 0.1f, 1.0f} : Color{0.15f, 0.16f, 0.2f, 1.0f};
                    if (uiButton(mx, my, 220, 36, upper(map), bg)) {
                        selectedMap = map;
                    }
                    ++col;
                    mx += 232;
                    if (col >= 5) { col = 0; mx = 24; my += 44; }
                }

                bool canGo = !selectedMap.empty();
                Color goColor = canGo ? Color{0.15f, 0.55f, 0.2f, 1.0f} : Color{0.3f, 0.3f, 0.3f, 1.0f};
                if (uiButton(24, kHeight - 80, 200, 44, "GO", goColor) && canGo) {
                    launchMap(menuExeDir, cstrikeDir, selectedMap);
                }
            }

        } else if (screen == Screen::Watch) {
            const char* msg = "WATCH - COMING SOON";
            float w = uiTextWidth(msg, 3.0f);
            uiDrawText((kWidth - w) / 2.0f, kHeight / 2.0f - 20, msg, Color{0.5f, 0.5f, 0.55f, 1.0f}, 3.0f);

        } else if (screen == Screen::Inventory) {
            uiDrawText(24, 84, "INVENTORY", kColorWhite, 3.0f);

            // Unopened cases: aggregate by name, each with an OPEN button.
            std::map<std::string, int> caseCounts;
            for (auto& n : econ.ownedCaseNames) caseCounts[n]++;
            float cx = 24, cy = 130;
            for (auto& [name, count] : caseCounts) {
                uiDrawRect(cx, cy, 220, 70, Color{0.15f, 0.16f, 0.2f, 1.0f});
                uiDrawText(cx + 10, cy + 8, upper(name), kColorWhite, 1.5f);
                char cntStr[16]; std::snprintf(cntStr, sizeof(cntStr), "x%d", count);
                uiDrawText(cx + 10, cy + 28, cntStr, Color{0.7f, 0.7f, 0.7f, 1.0f}, 1.5f);
                if (uiButton(cx + 10, cy + 44, 200, 20, "OPEN", Color{0.7f, 0.3f, 0.1f, 1.0f})) {
                    for (auto& c : cases) {
                        if (c.name == name) {
                            openingCase = &c;
                            revealDone = false;
                            spinTimer = 0.0f;
                            screen = Screen::CaseOpening;
                        }
                    }
                }
                cx += 232;
            }

            // Owned skins grid.
            float gx = 24, gy = 220;
            int col = 0;
            for (auto& item : econ.inventory) {
                float x = gx + col * 168;
                float y = gy;
                Color rc = rarityColor(item.skin.rarity);
                uiDrawRect(x, y, 156, 130, Color{0.1f, 0.1f, 0.12f, 1.0f});
                uiDrawRectOutline(x, y, 156, 130, rc, 3.0f);
                drawIconQuad(icons.get(item.skin), x + 18, y + 10, 120, 80);
                uiDrawText(x + 8, y + 96, upper(item.skin.weapon), kColorWhite, 1.2f);
                uiDrawText(x + 8, y + 112, item.skin.skinName, rc, 1.0f);
                ++col;
                if (col >= 7) { col = 0; gy += 140; }
            }
            if (econ.inventory.empty()) {
                uiDrawText(24, 220, "NO SKINS YET - OPEN A CASE FROM THE STORE", Color{0.5f, 0.5f, 0.5f, 1.0f}, 1.5f);
            }

        } else if (screen == Screen::Store) {
            uiDrawText(24, 84, "STORE", kColorWhite, 3.0f);
            uiDrawText(24, 116, "CASES ARE PURCHASED WITH FICTIVE COINS - NO REAL MONEY", Color{0.6f, 0.6f, 0.6f, 1.0f}, 1.2f);

            float cx = 24, cy = 150;
            for (auto& c : cases) {
                uiDrawRect(cx, cy, 260, 220, Color{0.13f, 0.14f, 0.17f, 1.0f});
                uiDrawRectOutline(cx, cy, 260, 220, Color{0.3f, 0.3f, 0.35f, 1.0f}, 2.0f);
                uiDrawText(cx + 14, cy + 14, upper(c.name), kColorWhite, 1.6f);

                // Small preview icons of what's inside.
                for (size_t i = 0; i < c.pool.size() && i < 4; ++i) {
                    drawIconQuad(icons.get(c.pool[i]), cx + 14 + (float)(i % 4) * 58, cy + 44, 50, 34);
                }

                char priceStr[32];
                std::snprintf(priceStr, sizeof(priceStr), "$ %d", c.price);
                uiDrawText(cx + 14, cy + 150, priceStr, Color{0.4f, 1.0f, 0.4f, 1.0f}, 2.0f);

                bool canAfford = econ.balance >= c.price;
                Color btnColor = canAfford ? Color{0.15f, 0.55f, 0.2f, 1.0f} : Color{0.3f, 0.3f, 0.3f, 1.0f};
                if (uiButton(cx + 14, cy + 180, 232, 30, "BUY", btnColor) && canAfford) {
                    buyCase(econ, c);
                }
                cx += 276;
            }

        } else if (screen == Screen::Settings) {
            uiDrawText(24, 84, "SETTINGS", kColorWhite, 3.0f);

            struct STab { const char* label; SettingsTab t; };
            STab stabs[] = {
                {"VIDEO", SettingsTab::Video}, {"AUDIO", SettingsTab::Audio},
                {"CONTROLS", SettingsTab::Controls}, {"MOUSE", SettingsTab::Mouse},
                {"CROSSHAIR", SettingsTab::Crosshair},
            };
            float stx = 24;
            for (auto& st : stabs) {
                float w = uiTextWidth(st.label, 1.4f) + 24;
                Color bg = (settingsTab == st.t) ? Color{0.7f, 0.3f, 0.1f, 1.0f} : Color{0.15f, 0.16f, 0.2f, 1.0f};
                if (uiButton(stx, 120, w, 32, st.label, bg)) settingsTab = st.t;
                stx += w + 6;
            }

            const float kLabelX = 24, kCtrlX = 260, kRowH = 46;
            float ry = 190;
            bool changed = false;

            auto row = [&](const char* label) {
                uiDrawText(kLabelX, ry + 8, label, Color{0.8f, 0.8f, 0.8f, 1.0f}, 1.3f);
                float y = ry;
                ry += kRowH;
                return y;
            };

            if (settingsTab == SettingsTab::Video) {
                struct Res { int w, h; };
                static const Res kResolutions[] = {{1024, 768}, {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}};
                int resIdx = 1;
                for (size_t i = 0; i < std::size(kResolutions); ++i) {
                    if (kResolutions[i].w == r_width.AsInt() && kResolutions[i].h == r_height.AsInt()) resIdx = (int)i;
                }
                {
                    float y = row("RESOLUTION");
                    char resStr[32];
                    std::snprintf(resStr, sizeof(resStr), "%d x %d", kResolutions[resIdx].w, kResolutions[resIdx].h);
                    if (uiButton(kCtrlX, y, 32, 32, "<", Color{0.2f, 0.2f, 0.24f, 1.0f})) {
                        resIdx = (resIdx - 1 + (int)std::size(kResolutions)) % (int)std::size(kResolutions);
                        r_width.SetFloat((float)kResolutions[resIdx].w);
                        r_height.SetFloat((float)kResolutions[resIdx].h);
                        changed = true;
                    }
                    uiDrawText(kCtrlX + 42, y + 8, resStr, kColorWhite, 1.4f);
                    if (uiButton(kCtrlX + 180, y, 32, 32, ">", Color{0.2f, 0.2f, 0.24f, 1.0f})) {
                        resIdx = (resIdx + 1) % (int)std::size(kResolutions);
                        r_width.SetFloat((float)kResolutions[resIdx].w);
                        r_height.SetFloat((float)kResolutions[resIdx].h);
                        changed = true;
                    }
                }
                {
                    float y = row("FULLSCREEN");
                    bool fs = r_fullscreen.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &fs, "ON", "OFF")) { r_fullscreen.SetFloat(fs ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("VSYNC");
                    bool vs = r_vsync.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &vs, "ON", "OFF")) { r_vsync.SetFloat(vs ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("FPS CAP");
                    float v = r_fpscap.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 260, 16, &v, 0.0f, 300.0f, "")) { r_fpscap.SetFloat(std::round(v)); changed = true; }
                    char s[16];
                    if (r_fpscap.AsInt() > 0) std::snprintf(s, sizeof(s), "%d", r_fpscap.AsInt());
                    else std::snprintf(s, sizeof(s), "UNCAPPED");
                    uiDrawText(kCtrlX + 276, y + 4, s, kColorWhite, 1.2f);
                }
                {
                    float y = row("FIELD OF VIEW");
                    float v = cl_fov.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 260, 16, &v, 60.0f, 110.0f, "")) { cl_fov.SetFloat(std::round(v)); changed = true; }
                    char s[16]; std::snprintf(s, sizeof(s), "%d", cl_fov.AsInt());
                    uiDrawText(kCtrlX + 276, y + 4, s, kColorWhite, 1.2f);
                }
                {
                    float y = row("TEXTURE FILTER");
                    bool linear = r_texfilter.AsBool();
                    if (uiToggle(kCtrlX, y, 160, 32, &linear, "SMOOTH", "PIXELATED")) { r_texfilter.SetFloat(linear ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("ANTI-ALIASING (MSAA)");
                    int aa = r_msaa.AsInt();
                    static const int kAaSteps[] = {0, 2, 4, 8};
                    int idx = 0;
                    for (size_t i = 0; i < std::size(kAaSteps); ++i) if (kAaSteps[i] == aa) idx = (int)i;
                    char s[16]; std::snprintf(s, sizeof(s), aa == 0 ? "OFF" : "%dx", aa);
                    if (uiButton(kCtrlX, y, 100, 32, s, Color{0.2f, 0.2f, 0.24f, 1.0f})) {
                        idx = (idx + 1) % (int)std::size(kAaSteps);
                        r_msaa.SetFloat((float)kAaSteps[idx]);
                        changed = true;
                    }
                    uiDrawText(kCtrlX + 116, y + 8, "(CLICK TO CYCLE - APPLIES ON NEXT LAUNCH)", Color{0.5f, 0.5f, 0.5f, 1.0f}, 0.9f);
                }
                {
                    float y = row("SHADOWS");
                    static const char* kShadowLabels[] = {"OFF", "LOW", "HIGH"};
                    int lvl = std::clamp(r_shadows.AsInt(), 0, 2);
                    if (uiButton(kCtrlX, y, 100, 32, kShadowLabels[lvl], Color{0.2f, 0.2f, 0.24f, 1.0f})) {
                        r_shadows.SetFloat((float)((lvl + 1) % 3));
                        changed = true;
                    }
                }
                {
                    float y = row("AMBIENT OCCLUSION");
                    bool ao = r_ao.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &ao, "ON", "OFF")) { r_ao.SetFloat(ao ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("PARTICLE QUALITY");
                    static const char* kPq[] = {"LOW", "MEDIUM", "HIGH"};
                    int lvl = std::clamp(r_particles.AsInt(), 0, 2);
                    if (uiButton(kCtrlX, y, 120, 32, kPq[lvl], Color{0.2f, 0.2f, 0.24f, 1.0f})) {
                        r_particles.SetFloat((float)((lvl + 1) % 3));
                        changed = true;
                    }
                }
                uiDrawText(kLabelX, ry + 12, "SHADOWS/AO ARE RESERVED SETTINGS - THIS RENDERER DOESN'T IMPLEMENT THEM YET", Color{0.5f, 0.4f, 0.3f, 1.0f}, 1.0f);

            } else if (settingsTab == SettingsTab::Audio) {
                struct VolRow { const char* label; Cvar* cvar; };
                VolRow vols[] = {
                    {"MASTER VOLUME", &vol_master}, {"MUSIC VOLUME", &vol_music},
                    {"EFFECTS VOLUME", &vol_effects}, {"VOICE VOLUME", &vol_voice}, {"UI VOLUME", &vol_ui},
                };
                for (auto& vr : vols) {
                    float y = row(vr.label);
                    float v = vr.cvar->AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 260, 16, &v, 0.0f, 1.0f, "")) { vr.cvar->SetFloat(v); changed = true; }
                    char s[16]; std::snprintf(s, sizeof(s), "%d%%", (int)(v * 100.0f));
                    uiDrawText(kCtrlX + 276, y + 4, s, kColorWhite, 1.2f);
                }
                uiDrawText(kLabelX, ry + 12, "NO AUDIO BACKEND YET - VOLUMES ARE SAVED FOR WHEN ONE EXISTS", Color{0.5f, 0.4f, 0.3f, 1.0f}, 1.0f);

            } else if (settingsTab == SettingsTab::Controls) {
                struct BindRow { const char* label; Cvar* cvar; };
                BindRow binds[] = {
                    {"MOVE FORWARD", &bind_forward}, {"MOVE BACK", &bind_back},
                    {"MOVE LEFT", &bind_left}, {"MOVE RIGHT", &bind_right},
                    {"JUMP", &bind_jump}, {"DUCK", &bind_duck},
                    {"RELOAD", &bind_reload}, {"USE / PLANT / DEFUSE", &bind_use},
                    {"BUY MENU", &bind_buymenu}, {"SCOREBOARD", &bind_scoreboard},
                    {"THIRD PERSON", &bind_thirdperson},
                };
                for (auto& br : binds) {
                    float y = row(br.label);
                    std::string keyLabel = (awaitingBind == br.cvar) ? "PRESS A KEY..." : upper(br.cvar->AsString());
                    Color bg = (awaitingBind == br.cvar) ? Color{0.7f, 0.5f, 0.1f, 1.0f} : Color{0.2f, 0.2f, 0.24f, 1.0f};
                    if (uiButton(kCtrlX, y, 200, 32, keyLabel, bg) && !awaitingBind) {
                        awaitingBind = br.cvar;
                    }
                }
                uiDrawText(kLabelX, ry + 12, "CLICK A KEY, THEN PRESS THE NEW KEY (ESC TO CANCEL)", Color{0.5f, 0.5f, 0.5f, 1.0f}, 1.0f);

            } else if (settingsTab == SettingsTab::Mouse) {
                {
                    float y = row("SENSITIVITY");
                    float v = sensitivity.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 260, 16, &v, 0.1f, 10.0f, "")) { sensitivity.SetFloat(v); changed = true; }
                    char s[16]; std::snprintf(s, sizeof(s), "%.2f", v);
                    uiDrawText(kCtrlX + 276, y + 4, s, kColorWhite, 1.2f);
                }
                {
                    float y = row("ADS SENSITIVITY");
                    float v = m_ads_sensitivity.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 260, 16, &v, 0.1f, 2.0f, "")) { m_ads_sensitivity.SetFloat(v); changed = true; }
                    char s[16]; std::snprintf(s, sizeof(s), "%.2f", v);
                    uiDrawText(kCtrlX + 276, y + 4, s, kColorWhite, 1.2f);
                }
                {
                    float y = row("INVERT MOUSE");
                    bool inv = m_pitch.AsFloat() < 0.0f;
                    if (uiToggle(kCtrlX, y, 140, 32, &inv, "ON", "OFF")) { m_pitch.SetFloat(inv ? -1.0f : 1.0f); changed = true; }
                }
                {
                    float y = row("RAW INPUT");
                    bool raw = m_rawinput.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &raw, "ON", "OFF")) { m_rawinput.SetFloat(raw ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("MOUSE ACCELERATION");
                    bool accel = m_customaccel.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &accel, "ON", "OFF")) { m_customaccel.SetFloat(accel ? 1.0f : 0.0f); changed = true; }
                }
                uiDrawText(kLabelX, ry + 12, "RAW INPUT TAKES EFFECT ON NEXT LAUNCH - THE REST APPLY LIVE", Color{0.5f, 0.5f, 0.5f, 1.0f}, 1.0f);

            } else if (settingsTab == SettingsTab::Crosshair) {
                {
                    float y = row("RED");
                    float v = cl_crosshair_r.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 200, 16, &v, 0.0f, 255.0f, "")) { cl_crosshair_r.SetFloat(std::round(v)); changed = true; }
                }
                {
                    float y = row("GREEN");
                    float v = cl_crosshair_g.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 200, 16, &v, 0.0f, 255.0f, "")) { cl_crosshair_g.SetFloat(std::round(v)); changed = true; }
                }
                {
                    float y = row("BLUE");
                    float v = cl_crosshair_b.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 200, 16, &v, 0.0f, 255.0f, "")) { cl_crosshair_b.SetFloat(std::round(v)); changed = true; }
                }
                {
                    float y = row("SIZE");
                    float v = cl_crosshair_size.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 200, 16, &v, 1.0f, 30.0f, "")) { cl_crosshair_size.SetFloat(std::round(v)); changed = true; }
                }
                {
                    float y = row("THICKNESS");
                    float v = cl_crosshair_thickness.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 200, 16, &v, 1.0f, 10.0f, "")) { cl_crosshair_thickness.SetFloat(std::round(v)); changed = true; }
                }
                {
                    float y = row("GAP");
                    float v = cl_crosshair_gap.AsFloat();
                    if (uiSlider(kCtrlX, y + 8, 200, 16, &v, 0.0f, 20.0f, "")) { cl_crosshair_gap.SetFloat(std::round(v)); changed = true; }
                }
                {
                    float y = row("OUTLINE");
                    bool ol = cl_crosshair_outline.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &ol, "ON", "OFF")) { cl_crosshair_outline.SetFloat(ol ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("CENTER DOT");
                    bool dot = cl_crosshair_dot.AsBool();
                    if (uiToggle(kCtrlX, y, 140, 32, &dot, "ON", "OFF")) { cl_crosshair_dot.SetFloat(dot ? 1.0f : 0.0f); changed = true; }
                }
                {
                    float y = row("DYNAMIC");
                    bool dyn = cl_crosshair_dynamic.AsBool();
                    if (uiToggle(kCtrlX, y, 200, 32, &dyn, "DYNAMIC", "STATIC")) { cl_crosshair_dynamic.SetFloat(dyn ? 1.0f : 0.0f); changed = true; }
                }

                // Live preview against a neutral backdrop.
                float previewX = 780, previewY = 260, previewSize = 200;
                uiDrawRect(previewX, previewY, previewSize, previewSize, Color{0.08f, 0.09f, 0.11f, 1.0f});
                uiDrawRectOutline(previewX, previewY, previewSize, previewSize, Color{0.3f, 0.3f, 0.35f, 1.0f}, 2.0f);
                uiDrawText(previewX, previewY - 20, "PREVIEW", Color{0.6f, 0.6f, 0.6f, 1.0f}, 1.2f);
                {
                    Color xc{
                        std::clamp(cl_crosshair_r.AsFloat() / 255.0f, 0.0f, 1.0f),
                        std::clamp(cl_crosshair_g.AsFloat() / 255.0f, 0.0f, 1.0f),
                        std::clamp(cl_crosshair_b.AsFloat() / 255.0f, 0.0f, 1.0f),
                        1.0f,
                    };
                    float pcx = previewX + previewSize / 2.0f, pcy = previewY + previewSize / 2.0f;
                    float size = std::max(1.0f, cl_crosshair_size.AsFloat());
                    float thick = std::max(1.0f, cl_crosshair_thickness.AsFloat());
                    float gap = std::max(0.0f, cl_crosshair_gap.AsFloat());
                    bool outline = cl_crosshair_outline.AsBool();
                    auto drawLeg = [&](float x, float y, float w, float h) {
                        if (outline) uiDrawRect(x - 1, y - 1, w + 2, h + 2, kColorBlack);
                        uiDrawRect(x, y, w, h, xc);
                    };
                    drawLeg(pcx - thick / 2.0f, pcy - gap - size, thick, size);
                    drawLeg(pcx - thick / 2.0f, pcy + gap, thick, size);
                    drawLeg(pcx - gap - size, pcy - thick / 2.0f, size, thick);
                    drawLeg(pcx + gap, pcy - thick / 2.0f, size, thick);
                    if (cl_crosshair_dot.AsBool()) drawLeg(pcx - thick / 2.0f, pcy - thick / 2.0f, thick, thick);
                }
            }

            if (changed) SettingsSave();

        } else if (screen == Screen::CaseOpening) {
            uiDrawText(24, 84, "OPENING CASE...", kColorWhite, 3.0f);

            if (!revealDone) {
                spinTimer += dt;
                // Spin through random pool entries for visual feedback, then commit.
                if (openingCase && !openingCase->pool.empty()) {
                    const SkinDef& spinning = openingCase->pool[(int)(spinTimer * 12.0f) % openingCase->pool.size()];
                    drawIconQuad(icons.get(spinning), kWidth / 2.0f - 90, 240, 180, 120);
                    uiDrawText(kWidth / 2.0f - uiTextWidth(spinning.skinName) / 2.0f, 370, spinning.skinName, kColorWhite, 1.5f);
                }
                if (spinTimer >= kSpinDuration && openingCase) {
                    openCase(econ, *openingCase, revealResult);
                    revealDone = true;
                }
            } else {
                Color rc = rarityColor(revealResult.skin.rarity);
                uiDrawRectOutline(kWidth / 2.0f - 100, 230, 200, 140, rc, 4.0f);
                drawIconQuad(icons.get(revealResult.skin), kWidth / 2.0f - 90, 240, 180, 120);
                std::string title = upper(revealResult.skin.weapon) + " | " + revealResult.skin.skinName;
                uiDrawText(kWidth / 2.0f - uiTextWidth(title, 1.5f) / 2.0f, 378, title, kColorWhite, 1.5f);
                uiDrawText(kWidth / 2.0f - uiTextWidth(rarityName(revealResult.skin.rarity)) / 2.0f, 398, rarityName(revealResult.skin.rarity), rc, 1.2f);

                if (uiButton(kWidth / 2.0f - 80, 440, 160, 36, "COLLECT", Color{0.7f, 0.3f, 0.1f, 1.0f})) {
                    screen = Screen::Inventory;
                    openingCase = nullptr;
                }
            }
        }

        uiEndFrame();
        SDL_GL_SwapWindow(window);

        if (!screenshotPath.empty()) {
            std::vector<uint8_t> pixels(kWidth * kHeight * 3);
            glReadPixels(0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::vector<uint8_t> flipped(kWidth * kHeight * 3);
            for (int y = 0; y < kHeight; ++y) {
                for (int x = 0; x < kWidth * 3; ++x) flipped[y * kWidth * 3 + x] = pixels[(kHeight - 1 - y) * kWidth * 3 + x];
            }
            SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(flipped.data(), kWidth, kHeight, 24, kWidth * 3, 0x0000FF, 0x00FF00, 0xFF0000, 0);
            if (surf) { SDL_SaveBMP(surf, screenshotPath.c_str()); SDL_FreeSurface(surf); }
            running = false;
        }
    }

    SettingsSave();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
