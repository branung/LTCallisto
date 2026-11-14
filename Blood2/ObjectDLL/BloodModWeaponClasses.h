// BloodModWeaponClasses.h: Blood II's weapon classes' constants, overridable by a mod.
//
// A mod ships Mod\WeaponClasses.txt:
//
//     [SHOTGUN]
//     Chrome     = 0.09
//     PickupAmmo = 2.0
//
//     [VOODOO]
//     VoodooHits = "FIRE SINGULARITY ELECTRIC NORMAL ACID"
//
// Mod\Weapons.txt is g_WeaponDefaults, the table both modules read. These are the rest: what
// each weapon class's constructor, its pickup's constructor, or a literal in its code sets, which
// the server alone reads. A section is a weapon's WEAP_ name without the prefix, as in
// Weapons.txt, a mod's new weapons too. A new weapon is made as its Base's class, so it starts
// from that class's values.
//
// A weapon's own values replace its constructor's in CWeapon::Init, which every weapon runs once
// the inventory makes it, the player's and an AI's alike:
//
//     Chrome            the view model's environment map alpha (0.05); only
//                       the weapons whose class environment-maps it have one.
//                       It has no visible effect yet: DECompat's
//                       FLAG_ENVIRONMENTMAP is 0 (de_types.h), so CWeapon
//                       never takes the branch that reads it.
//     MultiDamageBoost  TRUE doubles its damage in a multiplayer game
//     DamageType        what its own hits do: a DAMAGE_TYPE_ name
//     AltAmmoType       the ammo its alt fire takes, an AMMO_ name; retail
//                       gives only the combat shotgun and the assault rifle
//                       one of their own (DIEBUGDIE), and every other
//                       weapon's alt takes AmmoType
//
// Its pickup's (WeaponPickups.cpp), at the pickup's MID_PRECREATE, for the weapons that have a
// pickup:
//
//     PickupAmmo        the ammo the pickup gives with the weapon
//     PickupName        the name the pickup says, at most 63 characters
//
// Literals in one class's code, with their retail values in Blood2/ModReference/WeaponClasses.txt
// (testBlood2Tables --dump weaponclasses):
//
//     VOODOO            VoodooHits: the five hit types a primary stab picks
//                       from at random, DAMAGE_TYPE_ names; VoodooAltHit: the
//                       alt's; VoodooFX: the effect WonkyThemAll puts on each
//                       victim, an OBJFX_ name
//     PROXIMITYBOMB, TIMEBOMB
//                       HandExplosionType (an EXP_ name), HandExplosionSound,
//                       HandExplosionSoundRadius: the burst when the bomb goes
//                       off in the hand. Its thrown burst is the projectile
//                       class's (Mod\Projectiles.txt). The remote bomb's
//                       in-hand burst is commented out in retail's source.
//
// CWeapon::UpdateWeaponFX's, for a hit by BULLET, SHELL or BMG ammo:
//
//     MuzzleSmoke       its muzzle smokes: retail every weapon but the
//                       minigun
//     MuzzleLight       its muzzle lights up: retail none
//                       (both only on a shot that ejects a casing, which is
//                       the ammo's EjectShell in Mod\Ammo.txt)
//     Fragments         a hit on wood or stone throws fragments: retail
//                       every weapon but the minigun
//     HitFragments      a melee weapon's (MELEE, or a new weapon on it):
//                       every hit throws fragments; retail FALSE
//
// CWeapon::FireVector's, for every weapon that fires a vector:
//
//     Tracer            a LASER_ name without the prefix (Mod\Lasers.txt
//                       draws it), or NONE: retail NONE for every weapon
//
// Two literals in one class's firing, each TRUE in retail:
//
//     DEATHRAY          AltSweep: the alt fire's two rings and its damage to
//                       everything in a cone in front (CDeathRay::Fire);
//                       FALSE leaves the alt only its projectiles
//     FLAREGUN          SpreadPattern: a volley of several flares in the
//                       fixed eight-point pattern (CFlareGun::FireSpread);
//                       FALSE scatters them at random within the weapon's
//                       spread, as one flare always is
//
// [Options] Trace = TRUE in Mod\AI.txt prints, the first time each weapon is made, its
// constructor's values and the mod's. A key on a weapon that has not got it, an unknown name or a
// number below 0 loads nothing.

#ifndef __BLOODMODWEAPONCLASSES_H__
#define __BLOODMODWEAPONCLASSES_H__

#include <string>

namespace LT1Mod { class FileSource; }

enum BloodModWeaponClassKey
{
	WK_Chrome, WK_MultiDamageBoost, WK_DamageType, WK_AltAmmoType,		// every weapon
	WK_PickupAmmo, WK_PickupName,										// a weapon with a pickup
	WK_VoodooHits, WK_VoodooAltHit, WK_VoodooFX,						// VOODOO
	WK_HandExplosionType, WK_HandExplosionSound, WK_HandExplosionSoundRadius,	// PROXIMITYBOMB, TIMEBOMB
	WK_MuzzleSmoke, WK_MuzzleLight, WK_Fragments,						// every weapon
	WK_HitFragments,													// a melee weapon
	WK_Tracer,															// every weapon
	WK_AltSweep,														// DEATHRAY
	WK_SpreadPattern,													// FLAREGUN
	WK_COUNT
};

const char* BloodModWeaponClass_Key(int nKey);
// Whether weapon nWeapon (a WEAP_ id, a mod's new one too) has key nKey.
bool BloodModWeaponClass_Has(int nWeapon, int nKey);

// The mod's value for a weapon's key, if it gives one.
bool BloodModWeaponClass_Set(int nWeapon, int nKey);
float BloodModWeaponClass_ModFloat(int nWeapon, int nKey);
int BloodModWeaponClass_ModInt(int nWeapon, int nKey);		// BOOL and named keys
const char* BloodModWeaponClass_ModString(int nWeapon, int nKey);	// PickupName

// A literal's value: the mod's, or retail's.
int BloodModWeaponClass_VoodooHit(int nIndex);				// 0-4
int BloodModWeaponClass_VoodooAltHit();
int BloodModWeaponClass_VoodooFX();
int BloodModWeaponClass_HandExplosionType(int nWeapon);
const char* BloodModWeaponClass_HandExplosionSound(int nWeapon);
float BloodModWeaponClass_HandExplosionSoundRadius(int nWeapon);
bool BloodModWeaponClass_Flag(int nWeapon, int nKey);		// MuzzleSmoke .. HitFragments, AltSweep, SpreadPattern
// The LASER_ id a weapon's every shot draws from its muzzle to what it hits, or -1: retail none
int BloodModWeaponClass_Tracer(int nWeapon);

// A value as the file writes it, for the trace.
std::string BloodModWeaponClass_Format(int nKey, float f, int n, const char* s);
std::string BloodModWeaponClass_Summary(int nWeapon);		// what the mod gives one weapon
bool BloodModWeaponClass_FirstOf(int nWeapon, bool bPickup);	// once per weapon (and pickup) per load

bool BloodModWeaponClasses_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModWeaponClasses_Clear();
std::string BloodModWeaponClasses_Summary();
std::string BloodModWeaponClasses_Reference();
void BloodModWeaponClasses_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODWEAPONCLASSES_H__
