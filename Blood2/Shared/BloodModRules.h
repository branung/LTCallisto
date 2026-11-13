// BloodModRules.h: Blood II's loose constants, overridable by a mod.
//
// A mod ships Mod\Rules.txt:
//
//     [Player]
//     InvisibleAlpha = 0.13
//     SpawnItems     = "FLASHLIGHT"
//
//     [Corpse]
//     BurnFX = "FLAMING_2 FLAMING_3"
//
// The constants that belong to no table: a #define or a literal in one function's code, each
// moved into gRules with retail's value, which the code reads. A mod's value is written into
// gRules once when the module loads the mod, over a retail copy, as the tables' are
// (BloodModTable.h): every load starts from retail and an error leaves it. A section is an area
// of the game, matched without case. Each has its own keys, with retail's values in
// Blood2/ModReference/Rules.txt (testBlood2Tables --dump rules):
//
//   [Player]   InvisibleAlpha   the player's alpha while invisible (0.08)
//              UseDistance      how far the use key reaches (82)
//              IdleVoiceTime    seconds idle before the idle line (60)
//              SpawnItems       INV_ names the player gets on entering a
//                               world: retail none
//              SpawnAmmo        AMMO_ name and count pairs, the same:
//                               retail "BULLET 50"
//   [Items]    FlashlightChargeTime, GogglesChargeTime
//                               seconds a full charge lasts (120, 30)
//   [Impacts]  BloodSpurtDamage a flesh hit spurts blood above this
//                               damage, and one hit in four besides (25)
//   [Corpse]   BurnFX, SmokeFX  OBJFX_ names, one picked at random, for a
//                               burning and a smoking corpse (FLAMING_2,
//                               SMOKING_2)
//              GibOnExplosionOnly
//                               TRUE: an explosive hit gibs a corpse
//                               whatever its health, and no other hit gibs
//                               it or takes a limb (FALSE: it gibs when
//                               its damage kills it, and a hit may take
//                               the limb it struck)
//              GibHeadChance, GibArmChance, GibLegChance
//                               percent: the head, each arm and each leg a
//                               gibbed corpse throws (100, 80, 60)
//
// The server alone reads these. An unknown section or key, a value of the wrong kind or out of
// range loads nothing, and the error says where.

#ifndef __BLOODMODRULES_H__
#define __BLOODMODRULES_H__

#include <string>
#include <vector>

namespace LT1Mod { class FileSource; }

struct BloodModRulesValues
{
	// [Player]
	float		fInvisibleAlpha;
	float		fUseDistance;
	float		fIdleVoiceTime;
	const char*	szSpawnItems;
	const char*	szSpawnAmmo;
	// [Items]
	float		fFlashlightChargeTime;
	float		fGogglesChargeTime;
	// [Impacts]
	float		fBloodSpurtDamage;
	// [Corpse]
	const char*	szBurnFX;
	const char*	szSmokeFX;
	int			bGibOnExplosionOnly;
	int			nGibHeadChance;
	int			nGibArmChance;
	int			nGibLegChance;
};

extern BloodModRulesValues gRules;

// The keys, in file order; NULL past the last. A key's section, and its value as the file writes
// it, the current one or retail's.
const char* BloodModRules_Key(int nKey);
const char* BloodModRules_Section(int nKey);
std::string BloodModRules_Value(int nKey, bool bRetail = false);

// A list of OBJFX_ names (the corpse's FX keys) as ids; false, and pWhy, on a
// name that is not one.  An id's name, or NULL.
bool BloodModRules_ObjFXList(const char* pList, std::vector<int>& out, std::string* pWhy = 0);
int BloodMod_ObjFX(const char* pName);
const char* BloodMod_ObjFXName(int nId);

bool BloodModRules_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModRules_Clear();
std::string BloodModRules_Summary();
std::string BloodModRules_Reference();
void BloodModRules_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODRULES_H__
