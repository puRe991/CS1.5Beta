#pragma once

// Weapon catalog for the in-round buy menu and combat logic. Prices,
// damage, magazine sizes, and fire rates are the functional game balance
// numbers CS is generally known for (not creative/copyrightable text) —
// reused here as reasonable defaults, not copied from any source file.
// Model filenames match real CS 1.6-era asset basenames (the roster CS 1.5
// was converging toward); only 8 of them (the ones already listed before
// this roster expanded) have been verified to actually load against real
// game files — the rest follow the same naming convention and fail
// gracefully (equipWeapon() just no-ops) if the asset isn't present.
enum class WeaponCategory { Melee, Pistol, Smg, Shotgun, Rifle, Sniper, Heavy };

struct WeaponDef {
    const char* name;
    WeaponCategory category;
    int price;
    const char* viewModel; // v_<x>.mdl
    int magazineSize;      // 0 for melee (no ammo at all)
    int reserveAmmo;       // max carried reserve, not counting the loaded mag
    int damage;             // base hit damage, no range falloff modeled yet
    float fireRateRpm;       // rounds/attacks per minute; gates the fire-cooldown timer
    bool fullAuto;            // true = holding the mouse button keeps firing, false = click-per-shot
    float moveSpeedScale;      // fraction of the base move speed while this weapon is equipped
};

inline const WeaponDef kWeaponCatalog[] = {
    // name          category                  price  view model            mag  reserve dmg  rpm    auto   speed
    {"KNIFE",        WeaponCategory::Melee,     0,     "v_knife.mdl",        0,   0,      65,  120.0f, false, 1.0f},
    {"GLOCK 18",     WeaponCategory::Pistol,    400,   "v_glock18.mdl",      20,  120,    25,  400.0f, false, 1.0f},
    {"USP",          WeaponCategory::Pistol,    500,   "v_usp.mdl",          12,  100,    34,  343.0f, false, 1.0f},
    {"P228",         WeaponCategory::Pistol,    600,   "v_p228.mdl",         13,  104,    32,  400.0f, false, 1.0f},
    {"FIVE-SEVEN",   WeaponCategory::Pistol,    750,   "v_fiveseven.mdl",    20,  100,    20,  400.0f, false, 1.0f},
    {"DEAGLE",       WeaponCategory::Pistol,    650,   "v_deagle.mdl",       7,   35,     54,  267.0f, false, 1.0f},
    {"DUAL ELITES",  WeaponCategory::Pistol,    800,   "v_elite.mdl",        30,  120,    27,  500.0f, true,  1.0f},
    {"MP5 NAVY",     WeaponCategory::Smg,       1500,  "v_mp5.mdl",          30,  120,    25,  800.0f, true,  1.0f},
    {"TMP",          WeaponCategory::Smg,       1250,  "v_tmp.mdl",          30,  120,    20,  857.0f, true,  1.0f},
    {"P90",          WeaponCategory::Smg,       2350,  "v_p90.mdl",          50,  100,    21,  800.0f, true,  1.0f},
    {"MAC-10",       WeaponCategory::Smg,       1400,  "v_mac10.mdl",        30,  100,    22,  900.0f, true,  1.0f},
    {"UMP45",        WeaponCategory::Smg,       1700,  "v_ump45.mdl",        25,  100,    30,  700.0f, true,  1.0f},
    {"M3",           WeaponCategory::Shotgun,   1700,  "v_m3.mdl",           8,   32,     50,  68.0f,  false, 0.97f},
    {"XM1014",       WeaponCategory::Shotgun,   3000,  "v_xm1014.mdl",       7,   32,     40,  240.0f, false, 0.97f},
    {"GALIL",        WeaponCategory::Rifle,     2000,  "v_galil.mdl",        35,  90,     30,  800.0f, true,  1.0f},
    {"FAMAS",        WeaponCategory::Rifle,     2250,  "v_famas.mdl",        25,  90,     30,  666.0f, true,  1.0f},
    {"AK47",         WeaponCategory::Rifle,     2500,  "v_ak47.mdl",         30,  90,     36,  600.0f, true,  1.0f},
    {"M4A1",         WeaponCategory::Rifle,     3100,  "v_m4a1.mdl",         30,  90,     33,  666.0f, true,  1.0f},
    {"SG552",        WeaponCategory::Rifle,     3500,  "v_sg552.mdl",        30,  90,     33,  750.0f, true,  1.0f},
    {"AUG",          WeaponCategory::Rifle,     3500,  "v_aug.mdl",          30,  90,     32,  692.0f, true,  1.0f},
    {"SCOUT",        WeaponCategory::Sniper,    2750,  "v_scout.mdl",        10,  90,     75,  210.0f, false, 1.0f},
    {"AWP",          WeaponCategory::Sniper,    4750,  "v_awp.mdl",          10,  30,     115, 41.0f,  false, 0.9f},
    {"G3SG1",        WeaponCategory::Sniper,    5000,  "v_g3sg1.mdl",        20,  90,     80,  240.0f, true,  0.9f},
    {"SG550",        WeaponCategory::Sniper,    4200,  "v_sg550.mdl",        30,  90,     70,  240.0f, true,  0.9f},
    {"M249",         WeaponCategory::Heavy,     5750,  "v_m249.mdl",         100, 100,    32,  750.0f, true,  0.85f},
};
constexpr int kWeaponCatalogCount = sizeof(kWeaponCatalog) / sizeof(kWeaponCatalog[0]);

constexpr int kStartingMoney = 800;
constexpr int kMaxMoney = 16000;
constexpr int kRoundMoneyReward = 1400; // flat per-round award (no real win/loss economy yet)

// Melee attacks trace a much shorter distance than a bullet — a fixed
// stand-in for real swing-arc/lunge-distance melee detection, which would
// need actual hitboxes (see the weapon-system-depth README TODO).
constexpr float kMeleeRange = 64.0f;

inline const WeaponDef& weaponByIndex(int index) {
    if (index < 0 || index >= kWeaponCatalogCount) return kWeaponCatalog[0];
    return kWeaponCatalog[index];
}
