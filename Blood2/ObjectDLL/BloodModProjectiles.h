// BloodModProjectiles.h: Blood II's projectile classes' constants, overridable by a mod.
//
// A mod ships Mod\Projectiles.txt:
//
//     [CGrenade]
//     LifeTime    = 2.0
//     BounceCount = 2
//
//     [CTimeBomb]
//     DamageType     = "FIRE"
//     ExplosionType  = "NAPALM_PRIMARY"
//     ExplosionSound = "Sounds\Weapons\napalm\altimpact.wav"
//
// A section is a projectile class's registered name, as a weapon's ProjectileClass names it
// (Mod\Weapons.txt), without case. kClasses lists the 36 (41 with the Nightmare Levels), and
// tests/Blood2Tables checks that list against every END_CLASS_DEFAULT in ObjectDLL whose parent
// is CProjectile.
//
// Each class sets its members in its own constructor, so a mod's value replaces the
// constructor's at CProjectile's MID_PRECREATE, which every projectile's chains to, before
// anything reads it. These keys every class has:
//
//     DamageType   what its hits do: a DAMAGE_TYPE_ name without the prefix
//     LifeTime     seconds before it removes itself (0: never)
//     Gravity      TRUE falls, FALSE flies straight (FLAG_GRAVITY)
//     TrailFX      the effect it trails, an OBJFX_ name ("NONE" for none)
//     Explosion    TRUE explodes where it ends
//
// and these only the classes that hold them:
//
//     BounceCount  CGrenade, CGasGrenade, CRockProjectile: bounces before it
//                  bursts. A grenade still bursts on a character it hits
//                  before its first bounce.
//     TimeDelay    CTimeBomb: seconds from its throw to its burst.
//
// [Options] Trace = TRUE in Mod\AI.txt prints, the first time each class is made, the values its
// constructor gave it, and then each object the mod changed.
//
// Class constants: other values are a class's alone (ProjectileConstants.h). Each is a key of the
// class that reads it, with its retail value in Blood2/ModReference/Projectiles.txt
// (testBlood2Tables --dump projectiles):
//
//     CGasGrenade      SmokeTime, GasDamage (the most a second's breath does),
//                      GasDamageRadius, GasDamageDelay, GasDamageType
//     CDeathRayProjectile  BounceDist
//     COrbProjectile   DetectRadius, DamageTime; COrbAltProjectile DamageTime
//     CSingularityProjectile, CSingularityAltProjectile  AttractTime (the alt
//                      waits twice it, as retail does)
//     CFlayerPortal    SearchRange, ChainInterval
//     CTimeBomb, CProximityBomb, CRemoteBomb
//                      ExplosionType (an EXP_ name), ExplosionSound,
//                      ExplosionSoundRadius
//     CSkullProjectile FlySoundRadius
//
// A key on a class that has not got it, a name the game has not, or a number below 0 loads
// nothing and names the key. The server alone loads it.

#ifndef __BLOODMODPROJECTILES_H__
#define __BLOODMODPROJECTILES_H__

#include <string>

namespace LT1Mod { class FileSource; }

// The classes, in kClasses' order.
#define BLOODMOD_PROJECTILES_BASE(X) \
	X(CGrenade) X(CTimeBomb) X(CProximityBomb) X(CRemoteBomb) X(CHowitzerShell) X(CHowitzerAltShell) \
	X(CHowitzerAltFrag) X(CNapalmProjectile) X(CNapalmAltProjectile) X(CNapalmFireball) \
	X(CBugSprayProjectile) X(CBugSprayAltProjectile) X(CDeathRayProjectile) X(CLeechPrimeProjectile) \
	X(CLeechAltProjectile) X(CFlareProjectile) X(CFlareAltProjectile) X(CFlareBurstProjectile) \
	X(COrbProjectile) X(COrbAltProjectile) X(CTeslaProjectile) X(CTeslaBoltProjectile) X(CTeslaBallProjectile) \
	X(CSingularityProjectile) X(CSingularityAltProjectile) X(CEnergyBlastProjectile) X(CFireballProjectile) \
	X(CGroundStrikeProjectile) X(CShockwaveProjectile) X(CBehemothShockwaveProjectile) \
	X(CShikariLoogieProjectile) X(CNagaSpikeProjectile) X(CNagaDebrisProjectile) X(CVomitProjectile) \
	X(CGooProjectile) X(CSkullProjectile)
#ifdef _ADD_ON
#define BLOODMOD_PROJECTILES_ADDON(X) \
	X(CGasGrenade) X(CFlayerChain) X(CFlayerPortal) X(CRockProjectile) X(CNightmareFireball)
#else
#define BLOODMOD_PROJECTILES_ADDON(X)
#endif

enum BloodModProjectileClass
{
#define X(c) BMP_##c,
	BLOODMOD_PROJECTILES_BASE(X)
	BLOODMOD_PROJECTILES_ADDON(X)
#undef X
	BMP_COUNT
};

enum BloodModProjectileKey
{
	// Every class's, applied at MID_PRECREATE.
	PK_DamageType, PK_LifeTime, PK_Gravity, PK_TrailFX, PK_Explosion,
	// Some classes' members, applied by the class (CProjectile::ModApplyClass).
	PK_BounceCount, PK_TimeDelay,
	// Class constants, read where they are used.
	PK_SmokeTime, PK_GasDamage, PK_GasDamageRadius, PK_GasDamageDelay, PK_GasDamageType,
	PK_BounceDist, PK_DetectRadius, PK_DamageTime, PK_AttractTime, PK_SearchRange, PK_ChainInterval,
	PK_ExplosionType, PK_ExplosionSound, PK_ExplosionSoundRadius, PK_FlySoundRadius,
	PK_COUNT
};

const char* BloodModProjectile_Class(int nClass);
int BloodModProjectile_FindClass(const char* pName);	// BMP_ index, or -1
const char* BloodModProjectile_Key(int nKey);
// Whether class nClass has key nKey at all.
bool BloodModProjectile_Has(int nClass, int nKey);

// The mod's value for a class's key, if it gives one.
bool BloodModProjectile_Set(int nClass, int nKey);
float BloodModProjectile_ModFloat(int nClass, int nKey);	// FLOAT and INT keys
int BloodModProjectile_ModInt(int nClass, int nKey);		// INT, BOOL and the named keys' values
const char* BloodModProjectile_ModString(int nClass, int nKey);	// PATH keys

// A class constant: the mod's value, or retail's.
float BloodModProjectile_Float(int nClass, int nKey);
int BloodModProjectile_Int(int nClass, int nKey);
const char* BloodModProjectile_String(int nClass, int nKey);

// Every-class keys, by member: what the mod gives class nClass is written over the constructor's,
// and the count of keys written. nGravity becomes 1 or 0 where the mod sets Gravity, for the
// caller to set or clear FLAG_GRAVITY.
int BloodModProjectile_Apply(int nClass, unsigned char& nDamageType, float& fLifeTime, int& nGravity,
							 unsigned int& dwTrailFX, unsigned int& bExplosion);
// A value as the file writes it, for the trace: "EXPLODE", "3.0", "TRUE".
std::string BloodModProjectile_Format(int nKey, float f, int n);
bool BloodModProjectile_FirstOfClass(int nClass);	// once per class per load
std::string BloodModProjectile_ClassSummary(int nClass);	// "LifeTime 2.0, BounceCount 2", or ""

bool BloodModProjectiles_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModProjectiles_Clear();
std::string BloodModProjectiles_Summary();
std::string BloodModProjectiles_Reference();
void BloodModProjectiles_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODPROJECTILES_H__
