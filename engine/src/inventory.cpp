#include "inventory.h"

#include <algorithm>
#include <cstdlib>

const char* rarityName(Rarity r) {
    switch (r) {
        case Rarity::MilSpec: return "MIL-SPEC";
        case Rarity::Restricted: return "RESTRICTED";
        case Rarity::Classified: return "CLASSIFIED";
        case Rarity::Covert: return "COVERT";
        case Rarity::Special: return "SPECIAL";
    }
    return "?";
}

Color rarityColor(Rarity r) {
    switch (r) {
        case Rarity::MilSpec: return Color{0.29f, 0.41f, 1.0f, 1.0f};    // blue
        case Rarity::Restricted: return Color{0.53f, 0.28f, 1.0f, 1.0f}; // purple
        case Rarity::Classified: return Color{0.83f, 0.17f, 0.90f, 1.0f}; // pink
        case Rarity::Covert: return Color{0.92f, 0.29f, 0.29f, 1.0f};    // red
        case Rarity::Special: return Color{1.0f, 0.84f, 0.0f, 1.0f};     // gold
    }
    return kColorWhite;
}

namespace {

// CS:GO-like rarity odds for a standard weapon case, as fractions of 1.0.
struct RarityOdds { Rarity rarity; float chance; };
constexpr RarityOdds kOdds[] = {
    {Rarity::MilSpec, 0.7992f},
    {Rarity::Restricted, 0.1598f},
    {Rarity::Classified, 0.0320f},
    {Rarity::Covert, 0.0064f},
    {Rarity::Special, 0.0026f},
};

SkinDef skin(const char* weapon, const char* name, Rarity r, float tr, float tg, float tb) {
    return SkinDef{weapon, name, r, {tr, tg, tb}};
}

} // namespace

std::vector<CaseDef> buildDefaultCases() {
    CaseDef rifleCase;
    rifleCase.name = "Rifle Case";
    rifleCase.price = 250;
    rifleCase.pool = {
        skin("ak47", "Blue Streak", Rarity::MilSpec, 0.4f, 0.5f, 1.1f),
        skin("ak47", "Urban Fade", Rarity::MilSpec, 0.7f, 0.7f, 0.6f),
        skin("m4a1", "Arctic Camo", Rarity::MilSpec, 0.85f, 0.9f, 1.0f),
        skin("m4a1", "Nightfall", Rarity::MilSpec, 0.35f, 0.35f, 0.5f),
        skin("ak47", "Vertigo", Rarity::Restricted, 0.3f, 0.9f, 0.9f),
        skin("m4a1", "Emberglow", Rarity::Restricted, 1.2f, 0.5f, 0.2f),
        skin("aug", "Violet Shift", Rarity::Restricted, 0.7f, 0.3f, 1.2f),
        skin("ak47", "Case Hardened", Rarity::Classified, 0.6f, 0.55f, 0.5f),
        skin("awp", "Dragon's Eye", Rarity::Classified, 1.1f, 0.2f, 0.2f),
        skin("m4a1", "Neon Rush", Rarity::Covert, 0.2f, 1.3f, 0.9f),
        skin("ak47", "Bloodshot", Rarity::Covert, 1.4f, 0.15f, 0.15f),
        skin("knife", "Golden Talon", Rarity::Special, 1.4f, 1.1f, 0.2f),
    };

    CaseDef pistolCase;
    pistolCase.name = "Sidearm Case";
    pistolCase.price = 150;
    pistolCase.pool = {
        skin("usp", "Steel Grip", Rarity::MilSpec, 0.6f, 0.6f, 0.65f),
        skin("glock18", "Toxic Slide", Rarity::MilSpec, 0.4f, 1.1f, 0.3f),
        skin("deagle", "Copper Edge", Rarity::MilSpec, 1.1f, 0.6f, 0.35f),
        skin("usp", "Midnight", Rarity::Restricted, 0.25f, 0.25f, 0.4f),
        skin("deagle", "Sunburst", Rarity::Restricted, 1.3f, 0.85f, 0.15f),
        skin("glock18", "Frostbite", Rarity::Classified, 0.5f, 0.85f, 1.2f),
        skin("deagle", "Crimson Reaper", Rarity::Covert, 1.3f, 0.1f, 0.2f),
        skin("knife", "Silver Fang", Rarity::Special, 0.85f, 0.85f, 0.9f),
    };

    return {rifleCase, pistolCase};
}

bool buyCase(PlayerEconomy& econ, const CaseDef& def) {
    if (econ.balance < def.price) return false;
    econ.balance -= def.price;
    econ.ownedCaseNames.push_back(def.name);
    return true;
}

bool openCase(PlayerEconomy& econ, const CaseDef& def, OwnedSkin& outResult) {
    auto it = std::find(econ.ownedCaseNames.begin(), econ.ownedCaseNames.end(), def.name);
    if (it == econ.ownedCaseNames.end()) return false;
    econ.ownedCaseNames.erase(it);

    float roll = (float)std::rand() / (float)RAND_MAX;
    Rarity chosen = Rarity::MilSpec;
    float cumulative = 0.0f;
    for (const auto& o : kOdds) {
        cumulative += o.chance;
        if (roll <= cumulative) { chosen = o.rarity; break; }
    }

    std::vector<const SkinDef*> candidates;
    for (const auto& s : def.pool) {
        if (s.rarity == chosen) candidates.push_back(&s);
    }
    if (candidates.empty()) {
        // Fall back to whatever the case actually has, if this rarity tier is empty.
        for (const auto& s : def.pool) candidates.push_back(&s);
    }
    const SkinDef* picked = candidates[std::rand() % candidates.size()];

    OwnedSkin owned{*picked};
    econ.inventory.push_back(owned);
    outResult = owned;
    return true;
}
