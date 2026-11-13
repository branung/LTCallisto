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
//
// and the client's, with the swim jump and the liquid mark read by both modules:
//
//   [Camera]   BobVertical, BobHorizontal, SwayVertical, SwayHorizontal
//                               the view's bob and the weapon's sway as
//                               the player walks (24, 16, 32, 80)
//              ShakeScale       a screen shake's magnitude (3)
//   [ViewWeapon] IdleDelayMin, IdleDelayMax
//                               seconds before a held weapon plays its
//                               idle animation, a random value between
//                               (20, 60)
//   [Movement] LadderStopTime   seconds a ladder (and a conveyor) takes to
//                               stop the player (0.2)
//              SwimJumpRepeat   TRUE: a swimmer can jump out of the water
//                               from the start and after every jump
//              SurfaceFloat     FALSE: breaking the surface never floats
//                               the player there (TRUE)
//   [Items]    FlashlightRadius the flashlight's light radius per unit of
//                               distance to what it lights (0.4)
//   [Impacts]  SplatScaleMin, SplatScaleMax, SpurtEmittersMin,
//              SpurtEmittersMax, BigMetalSparkTime, MetalSparkTime,
//              BloodSparkEmissionRadius, BloodSparkRadius, FragmentsMin,
//              FragmentsMax, BubblesInLiquid, SmokeDelay, SmokeScale,
//              BubbleGravity    WeaponFX.cpp's impact constants
//              LiquidMark       the mark a shot leaves on liquid: "" for
//                               the stone's, as retail
//   [Effects]  FleshGibChance, FleshGibMore, FleshGibScale
//                               at high detail, the percent chance a flesh
//                               gib burst is multiplied by FleshGibMore,
//                               and by FleshGibScale otherwise (20, 2, 1)
//              GeneralCap .. ObjectCap
//                               the most of each kind of effect alive at
//                               once (SFXMgr.cpp)
//   [Camera]   WonkyVision      FALSE: the screen never warbles, not for
//                               a gas grenade, a voodoo wonky, a thief on
//                               the player, or slow death
//   [ViewWeapon] DualAltAsPrimary
//                               TRUE: with two guns, alt fire fires both as
//                               primary (retail); FALSE fires their alts
//   [HUD]      WeaponTabsOnReset
//                               TRUE: the weapon tabs scroll in when the
//                               status bar resets (a level start)
//   [Menus]    VersionText      the main menu's version line: "" for the
//                               build's own
//              CustomLevelsMultiplayer
//                               FALSE: the custom level list leaves out the
//                               multiplayer worlds
//              AddonIntroMovie  TRUE: the add-on plays the intro movie
//                               after the logos, as the base game does
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
// Both modules load the file and read their own keys. An unknown section or key, a value of the
// wrong kind or out of range, or a minimum above its maximum loads nothing, and the error says
// where.

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
	// [Camera] (the client's)
	float		fBobVertical;
	float		fBobHorizontal;
	float		fSwayVertical;
	float		fSwayHorizontal;
	float		fShakeScale;
	int			bWonkyVision;
	// [ViewWeapon]
	float		fIdleDelayMin;
	float		fIdleDelayMax;
	int			bDualAltAsPrimary;
	// [Movement]
	float		fLadderStopTime;
	int			bSwimJumpRepeat;		// both modules
	int			bSurfaceFloat;
	// [Items]
	float		fFlashlightChargeTime;
	float		fGogglesChargeTime;
	float		fFlashlightRadius;		// the client's
	// [Impacts]
	float		fBloodSpurtDamage;
	float		fSplatScaleMin;			// the client's, to LiquidMark
	float		fSplatScaleMax;
	int			nSpurtEmittersMin;
	int			nSpurtEmittersMax;
	float		fBigMetalSparkTime;
	float		fMetalSparkTime;
	float		fBloodSparkEmissionRadius;
	float		fBloodSparkRadius;
	int			nFragmentsMin;
	int			nFragmentsMax;
	int			bBubblesInLiquid;
	float		fSmokeDelay;
	float		fSmokeScale;
	float		fBubbleGravity;
	const char*	szLiquidMark;			// both modules
	// [Corpse]
	const char*	szBurnFX;
	const char*	szSmokeFX;
	int			bGibOnExplosionOnly;
	int			nGibHeadChance;
	int			nGibArmChance;
	int			nGibLegChance;
	// [Effects] (the client's)
	int			nFleshGibChance;
	int			nFleshGibMore;
	int			nFleshGibScale;
	int			nCaps[8];				// SFXMgr's dynamic effect lists
	// [HUD] (the client's)
	int			bWeaponTabsOnReset;
	// [Menus]
	const char*	szVersionText;
	int			bCustomLevelsMultiplayer;
	int			bAddonIntroMovie;
	// [Cheats]
	int			bCheatInfiniteAmmo;		// "kfa" also makes ammo endless
	const char*	szCheatMoreWeapons;		// "kfa2"'s weapons, WEAP_ names
	const char*	szCheatAllItems;		// "giveallinv"'s items, INV_ names
	// [Voice]
	int			bWillpowerVoiceAtPickup;	// the power-up line on taking willpower, not its end
	int			bSuicideVoice;			// a suicide's own line, else the death line
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
