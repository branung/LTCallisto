// BloodModAmmo.h: Blood II's ammo types' table, overridable by a mod.
//
// A mod ships Mod\Ammo.txt:
//
//     [BULLET]
//     Max1 = 200.0
//     Pickup = 200.0
//
//     [FOCUS]
//     Regen1 = 4.0
//
// gAmmoValues (AmmoValues.cpp) holds the mod's value, written in place once when the server
// loads the mod, from a retail copy, as the weapons' table is (BloodModWeapons.h). A section is
// an AMMO_ name without the prefix (BULLET, SHELL, BMG, FLARE, DIEBUGDIE, HOWITZER, FUEL,
// BATTERY, FOCUS, PROXIMITYBOMB, REMOTEBOMB, TIMEBOMB), matched without case. The keys, with
// their retail values in Blood2/ModReference/Ammo.txt (testBlood2Tables --dump ammo):
//
//     Max1 .. Max6      the most a player carries at strength 1 .. 6, or
//                       magic for FOCUS (CInventoryMgr's caps)
//     Pickup            what the type's ammo pickup gives; the eight that
//                       have one, BULLET .. BATTERY
//     Regen1 .. Regen6  FOCUS only: what it regains a second, by magic
//     EjectShell        TRUE: a weapon whose ammo it is ejects a casing as
//                       it fires, and only then shows its muzzle smoke and
//                       light (Weapon.cpp); retail BULLET and SHELL
//
// A Pickup on a type with no pickup, or a Regen on any but FOCUS, is an error. The server alone
// loads it.

#ifndef __BLOODMODAMMO_H__
#define __BLOODMODAMMO_H__

#include <string>

namespace LT1Mod { class FileSource; }

// The section name of AMMO_ id nId, or NULL; an id by name, or AMMO_NONE.
const char* BloodModAmmo_Name(int nId);
int BloodModAmmo_Find(const char* pName);
const char* BloodModAmmo_Key(int nField);
std::string BloodModAmmo_Value(int nId, int nField, bool bRetail = false);

bool BloodModAmmo_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModAmmo_Clear();
std::string BloodModAmmo_Summary();
std::string BloodModAmmo_Reference();
void BloodModAmmo_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODAMMO_H__
