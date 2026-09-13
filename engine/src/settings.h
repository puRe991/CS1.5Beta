#pragma once

// Shared, persisted player settings: video, audio, mouse, crosshair and key
// bindings. Declared once here and linked into both cs15engine and csmenu so
// the Settings screen (csmenu) and the game (cs15engine) agree on cvar names
// and defaults; they talk to each other only through config.cfg on disk
// (SettingsLoad()/SettingsSave()), since they're separate processes.
//
// Video/mouse/crosshair/bindings are wired into an actual renderer or input
// path. Audio (volume_*) and r_shadows/r_ao have no backend yet in this
// engine (no sound system, no shadow/AO pass) — they're still real, archived
// cvars so a future audio/renderer pass can read them without adding new UI.

#include <SDL2/SDL.h>
#include "cvar.h"

// --- Video ---
extern Cvar cl_fov;
extern Cvar r_width;
extern Cvar r_height;
extern Cvar r_vsync;
extern Cvar r_fullscreen;
extern Cvar r_fpscap;
extern Cvar r_texfilter;
extern Cvar r_msaa;
extern Cvar r_shadows;
extern Cvar r_ao;
extern Cvar r_particles;

// --- Audio (reserved: stored/persisted, no audio backend to apply them to yet) ---
extern Cvar vol_master;
extern Cvar vol_music;
extern Cvar vol_effects;
extern Cvar vol_voice;
extern Cvar vol_ui;

// --- Mouse ---
extern Cvar sensitivity;
extern Cvar m_ads_sensitivity;
extern Cvar m_pitch;
extern Cvar m_rawinput;
extern Cvar m_customaccel;

// --- Crosshair ---
extern Cvar cl_crosshair_r;
extern Cvar cl_crosshair_g;
extern Cvar cl_crosshair_b;
extern Cvar cl_crosshair_size;
extern Cvar cl_crosshair_thickness;
extern Cvar cl_crosshair_gap;
extern Cvar cl_crosshair_outline;
extern Cvar cl_crosshair_dot;
extern Cvar cl_crosshair_dynamic;

// --- Key bindings (stored as SDL scancode names, e.g. "W", "Space", "Left Ctrl") ---
extern Cvar bind_forward;
extern Cvar bind_back;
extern Cvar bind_left;
extern Cvar bind_right;
extern Cvar bind_jump;
extern Cvar bind_duck;
extern Cvar bind_reload;
extern Cvar bind_thirdperson;
extern Cvar bind_scoreboard;
extern Cvar bind_buymenu;
extern Cvar bind_use;

// Resolves a bind_* cvar's stored key name back to a scancode, falling back
// to `fallback` if the stored name is empty/unrecognized (e.g. first run,
// or a hand-edited config with a typo).
SDL_Scancode SettingsScancodeFor(const Cvar& bindCvar, SDL_Scancode fallback);

// Both cs15engine and csmenu use the same on-disk file so a setting changed
// in the menu takes effect the next time the game is launched (and vice
// versa for anything the game itself can save, e.g. via the console).
extern const char* kSettingsConfigPath;
void SettingsLoad();
void SettingsSave();
