#include "settings.h"

Cvar cl_fov("fov", "90", CVAR_ARCHIVE, "horizontal-ish field of view, degrees");
Cvar r_width("width", "1280", CVAR_ARCHIVE, "window width in pixels");
Cvar r_height("height", "720", CVAR_ARCHIVE, "window height in pixels");
Cvar r_vsync("vsync", "1", CVAR_ARCHIVE, "1 = vsync on, 0 = off");
Cvar r_fullscreen("fullscreen", "0", CVAR_ARCHIVE, "1 = fullscreen (desktop resolution), 0 = windowed");
Cvar r_fpscap("fps_max", "0", CVAR_ARCHIVE, "frame rate cap in fps, 0 = uncapped");
Cvar r_texfilter("gl_texfilter", "1", CVAR_ARCHIVE, "1 = bilinear texture filtering, 0 = nearest (pixelated)");
Cvar r_msaa("gl_msaa", "0", CVAR_ARCHIVE, "multisample anti-aliasing samples: 0, 2, 4 or 8");
Cvar r_shadows("r_shadows", "0", CVAR_ARCHIVE, "dynamic shadows quality (0=off..2=high); reserved, no shadow pass yet");
Cvar r_ao("r_ao", "0", CVAR_ARCHIVE, "ambient occlusion (0=off,1=on); reserved, no AO pass yet");
Cvar r_particles("r_particle_quality", "2", CVAR_ARCHIVE, "particle effect quality: 0=low 1=medium 2=high");

Cvar vol_master("volume_master", "1.0", CVAR_ARCHIVE, "master volume 0-1; reserved, no audio backend yet");
Cvar vol_music("volume_music", "1.0", CVAR_ARCHIVE, "music volume 0-1; reserved, no audio backend yet");
Cvar vol_effects("volume_effects", "1.0", CVAR_ARCHIVE, "sound effects volume 0-1; reserved, no audio backend yet");
Cvar vol_voice("volume_voice", "1.0", CVAR_ARCHIVE, "voice chat volume 0-1; reserved, no audio backend yet");
Cvar vol_ui("volume_ui", "1.0", CVAR_ARCHIVE, "UI sound volume 0-1; reserved, no audio backend yet");

Cvar sensitivity("sensitivity", "3.0", CVAR_ARCHIVE, "mouse look speed multiplier");
Cvar m_ads_sensitivity("m_ads_sensitivity", "1.0", CVAR_ARCHIVE, "sensitivity multiplier while aiming down sights");
Cvar m_pitch("m_pitch", "1.0", CVAR_ARCHIVE, "vertical mouse look scale; negative inverts");
Cvar m_rawinput("m_rawinput", "1", CVAR_ARCHIVE, "1 = raw mouse input (bypass OS pointer accel), 0 = OS mouse settings");
Cvar m_customaccel("m_customaccel", "0", CVAR_ARCHIVE, "1 = enable engine-side mouse acceleration curve, 0 = flat sensitivity");

Cvar cl_crosshair_r("cl_crosshair_r", "0", CVAR_ARCHIVE, "crosshair color red 0-255");
Cvar cl_crosshair_g("cl_crosshair_g", "255", CVAR_ARCHIVE, "crosshair color green 0-255");
Cvar cl_crosshair_b("cl_crosshair_b", "0", CVAR_ARCHIVE, "crosshair color blue 0-255");
Cvar cl_crosshair_size("cl_crosshair_size", "8", CVAR_ARCHIVE, "crosshair line length in pixels");
Cvar cl_crosshair_thickness("cl_crosshair_thickness", "2", CVAR_ARCHIVE, "crosshair line thickness in pixels");
Cvar cl_crosshair_gap("cl_crosshair_gap", "4", CVAR_ARCHIVE, "crosshair gap from center in pixels");
Cvar cl_crosshair_outline("cl_crosshair_outline", "1", CVAR_ARCHIVE, "1 = draw a black outline around the crosshair");
Cvar cl_crosshair_dot("cl_crosshair_dot", "0", CVAR_ARCHIVE, "1 = draw a center dot");
Cvar cl_crosshair_dynamic("cl_crosshair_dynamic", "0", CVAR_ARCHIVE, "1 = gap widens while moving/firing, 0 = static gap");

Cvar bind_forward("bind_forward", "W", CVAR_ARCHIVE, "move forward key");
Cvar bind_back("bind_back", "S", CVAR_ARCHIVE, "move backward key");
Cvar bind_left("bind_left", "A", CVAR_ARCHIVE, "strafe left key");
Cvar bind_right("bind_right", "D", CVAR_ARCHIVE, "strafe right key");
Cvar bind_jump("bind_jump", "Space", CVAR_ARCHIVE, "jump key");
Cvar bind_duck("bind_duck", "Left Ctrl", CVAR_ARCHIVE, "duck/crouch key");
Cvar bind_reload("bind_reload", "R", CVAR_ARCHIVE, "reload key");
Cvar bind_thirdperson("bind_thirdperson", "V", CVAR_ARCHIVE, "toggle third-person view key");
Cvar bind_scoreboard("bind_scoreboard", "Tab", CVAR_ARCHIVE, "show scoreboard key");
Cvar bind_buymenu("bind_buymenu", "B", CVAR_ARCHIVE, "open buy menu key");
Cvar bind_use("bind_use", "E", CVAR_ARCHIVE, "use/plant/defuse key");

SDL_Scancode SettingsScancodeFor(const Cvar& bindCvar, SDL_Scancode fallback) {
    if (bindCvar.AsString().empty()) return fallback;
    SDL_Scancode sc = SDL_GetScancodeFromName(bindCvar.AsString().c_str());
    return sc != SDL_SCANCODE_UNKNOWN ? sc : fallback;
}

const char* kSettingsConfigPath = "config.cfg";

void SettingsLoad() { CvarSystem::Get().LoadConfig(kSettingsConfigPath); }
void SettingsSave() { CvarSystem::Get().SaveConfig(kSettingsConfigPath); }
