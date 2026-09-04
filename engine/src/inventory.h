#pragma once

#include <string>
#include <vector>

#include "ui/ui.h"

// Fictive in-game economy: skins, cases, and case-opening. All weapon skins
// here are our own invented names/colors applied as a tint over the real
// weapon textures — CS:GO's skin system didn't exist yet in the CS 1.5 era
// this engine otherwise reproduces, and we don't have (or copy) Valve's
// actual skin artwork or skin names.

enum class Rarity { MilSpec, Restricted, Classified, Covert, Special };

const char* rarityName(Rarity r);
Color rarityColor(Rarity r);

struct SkinDef {
    std::string weapon;   // matches the w_/v_ model basename, e.g. "ak47"
    std::string skinName; // our own invented name, e.g. "Blue Streak"
    Rarity rarity;
    float tint[3]; // multiplied onto the base weapon texture's RGB
};

struct CaseDef {
    std::string name;
    int price; // in fictive Coins
    std::vector<SkinDef> pool;
};

struct OwnedSkin {
    SkinDef skin;
};

struct PlayerEconomy {
    int balance = 5000; // starting fictive Coins
    std::vector<std::string> ownedCaseNames; // one entry per unopened case owned
    std::vector<OwnedSkin> inventory;
};

std::vector<CaseDef> buildDefaultCases();

bool buyCase(PlayerEconomy& econ, const CaseDef& def);
// Removes one owned instance of def.name and adds a randomly rolled skin to
// the inventory (weighted by rarity, CS:GO-like odds). Returns the result,
// or nullptr (via bool) if no case of that type was owned.
bool openCase(PlayerEconomy& econ, const CaseDef& def, OwnedSkin& outResult);
