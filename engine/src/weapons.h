#pragma once

// Weapon catalog for the in-round buy menu. Prices are the functional game
// balance numbers CS is generally known for (not creative/copyrightable
// text) — reused here as reasonable defaults, not copied from any source
// file. Model filenames match the real CS 1.5 asset basenames already
// verified to load (see the "alle Waffen" batch test).
struct WeaponDef {
    const char* name;
    int price;
    const char* viewModel; // v_<x>.mdl
    int magazineSize;
};

inline const WeaponDef kWeaponCatalog[] = {
    {"KNIFE",    0,    "v_knife.mdl",     0},
    {"GLOCK 18", 400,  "v_glock18.mdl",   20},
    {"USP",      500,  "v_usp.mdl",       12},
    {"DEAGLE",   650,  "v_deagle.mdl",    7},
    {"MP5",      1500, "v_mp5.mdl",       30},
    {"M4A1",     3100, "v_m4a1.mdl",      30},
    {"AK47",     2500, "v_ak47.mdl",      30},
    {"AWP",      4750, "v_awp.mdl",       10},
};
constexpr int kWeaponCatalogCount = sizeof(kWeaponCatalog) / sizeof(kWeaponCatalog[0]);

constexpr int kStartingMoney = 800;
constexpr int kMaxMoney = 16000;
constexpr int kRoundMoneyReward = 1400; // flat per-round award (no real win/loss economy yet)
