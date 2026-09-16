#include "test_framework.h"

#include <string>

#include "weapons.h"

TEST(weapon_catalog_covers_every_category) {
    bool seen[7] = {false, false, false, false, false, false, false};
    for (int i = 0; i < kWeaponCatalogCount; ++i) {
        seen[(int)kWeaponCatalog[i].category] = true;
    }
    for (bool s : seen) CHECK(s);
}

TEST(weapon_catalog_entries_are_internally_consistent) {
    for (int i = 0; i < kWeaponCatalogCount; ++i) {
        const WeaponDef& w = kWeaponCatalog[i];
        CHECK(w.price >= 0);
        CHECK(w.fireRateRpm > 0.0f);
        CHECK(w.moveSpeedScale > 0.0f);
        bool isMelee = w.category == WeaponCategory::Melee;
        CHECK_EQ(isMelee, w.magazineSize == 0);
        if (!isMelee) {
            CHECK(w.magazineSize > 0);
            CHECK(w.reserveAmmo >= w.magazineSize);
        } else {
            CHECK_EQ(w.reserveAmmo, 0);
        }
        CHECK(w.damage > 0);
    }
}

TEST(weapon_by_index_clamps_out_of_range_to_first_entry) {
    CHECK_EQ(std::string(weaponByIndex(-1).name), std::string(kWeaponCatalog[0].name));
    CHECK_EQ(std::string(weaponByIndex(kWeaponCatalogCount).name), std::string(kWeaponCatalog[0].name));
    CHECK_EQ(std::string(weaponByIndex(1000).name), std::string(kWeaponCatalog[0].name));
}

TEST(weapon_by_index_returns_the_matching_entry) {
    for (int i = 0; i < kWeaponCatalogCount; ++i) {
        CHECK_EQ(std::string(weaponByIndex(i).name), std::string(kWeaponCatalog[i].name));
    }
}

int main() { return RUN_ALL_TESTS(); }
