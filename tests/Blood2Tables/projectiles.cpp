// projectiles.cpp: testBlood2Tables for Mod\Projectiles.txt (BloodModProjectiles.h).
//
//   classes       kClasses is every class ObjectDLL registers with CProjectile
//                 as its parent, 36, or 41 with the Nightmare Levels.
//   constants     with no mod each class constant is ProjectileConstants.h's.
//   members       every class takes the five every-class keys, and Apply
//                 writes each over the constructor's value and only it.
//   keys          each class-only key loads on its classes and is refused,
//                 by name, on every other.
//   round trip    the --dump file loads as a mod and changes no constant.
//   errors        a bad file loads nothing, over a mod loaded before it.

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "ProjectileConstants.h"
#include "BloodModProjectiles.h"
#include "harness.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>

namespace
{

bool LoadProjectiles(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\Projectiles.txt", text, err, BloodModProjectiles_Load);
}

void Unload()
{
	BloodModProjectiles_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// Every class constant's value now, "CGasGrenade.SmokeTime 10.0", for those
// that differ from a with-no-mod snapshot.
std::vector<std::string> g_Retail;
std::vector<std::string> Constants()
{
	std::vector<std::string> out;
	for (int c = 0; c < BMP_COUNT; ++c)
		for (int k = PK_SmokeTime; k < PK_COUNT; ++k)
			if (BloodModProjectile_Has(c, k))
			{
				char b[256];
				snprintf(b, sizeof(b), "%s.%s %g %d %s", BloodModProjectile_Class(c), BloodModProjectile_Key(k),
						 BloodModProjectile_Float(c, k), BloodModProjectile_Int(c, k), BloodModProjectile_String(c, k));
				out.push_back(b);
			}
	return out;
}
std::vector<std::string> Differ()
{
	const std::vector<std::string> now = Constants();
	std::vector<std::string> out;
	for (size_t i = 0; i < now.size(); ++i)
		if (i >= g_Retail.size() || now[i] != g_Retail[i]) out.push_back(now[i]);
	return out;
}

void TestClasses()
{
	printf("projectiles: classes\n");
	std::string why;
	const std::set<std::string> scanned = ScanClasses(BLOOD2_OBJECTDLL_DIR, why, "CProjectile");
	std::set<std::string> listed;
	for (int i = 0; BloodModProjectile_Class(i); ++i) listed.insert(BloodModProjectile_Class(i));
	std::vector<std::string> missing, extra;
	for (std::set<std::string>::const_iterator it = scanned.begin(); it != scanned.end(); ++it)
		if (!listed.count(*it)) missing.push_back(*it);
	for (std::set<std::string>::const_iterator it = listed.begin(); it != listed.end(); ++it)
		if (!scanned.count(*it)) extra.push_back(*it);
	char b[256];
	snprintf(b, sizeof(b), "the %d classes listed are the %d ObjectDLL registers with CProjectile as their parent "
			 "(missing: %s; extra: %s)%s", (int)listed.size(), (int)scanned.size(), Join(missing).c_str(),
			 Join(extra).c_str(), why.c_str());
	Check(!scanned.empty() && missing.empty() && extra.empty(), b);
#ifdef _ADD_ON
	Check(listed.size() == 41 && listed.count("CGasGrenade") && listed.count("CFlayerPortal"),
		  "41 with the Nightmare Levels, CGasGrenade and CFlayerPortal among them");
#else
	Check(listed.size() == 36 && !listed.count("CGasGrenade"), "36 in the base game, without the add-on's CGasGrenade");
#endif
	Check(BloodModProjectile_FindClass("cgrenade") == BMP_CGrenade && BloodModProjectile_FindClass("CProjectile") < 0 &&
		  BloodModProjectile_FindClass("Grenade") < 0, "names are matched without case and whole; CProjectile is not one");
}

void TestConstants()
{
	printf("projectiles: constants\n");
	Unload();
	Check(BloodModProjectile_Float(BMP_CDeathRayProjectile, PK_BounceDist) == DEATHRAY_BOUNCE_DIST &&
		  BloodModProjectile_Float(BMP_COrbProjectile, PK_DetectRadius) == THEORB_DETECT_RADIUS &&
		  BloodModProjectile_Float(BMP_COrbProjectile, PK_DamageTime) == THEORB_DAMAGE_TIME &&
		  BloodModProjectile_Float(BMP_COrbAltProjectile, PK_DamageTime) == THEORB_DAMAGE_TIME &&
		  BloodModProjectile_Float(BMP_CSingularityProjectile, PK_AttractTime) == SINGULARITY_ATTRACT_TIME &&
		  BloodModProjectile_Float(BMP_CSingularityAltProjectile, PK_AttractTime) == SINGULARITY_ATTRACT_TIME &&
		  BloodModProjectile_Float(BMP_CSkullProjectile, PK_FlySoundRadius) == SKULL_FLY_SOUND_RADIUS,
		  "with no mod the base game's constants are ProjectileConstants.h's");
	Check(BloodModProjectile_Int(BMP_CTimeBomb, PK_ExplosionType) == EXP_DEFAULT_SMALL &&
		  BloodModProjectile_Int(BMP_CProximityBomb, PK_ExplosionType) == EXP_DEFAULT_MEDIUM &&
		  BloodModProjectile_Int(BMP_CRemoteBomb, PK_ExplosionType) == EXP_DEFAULT_SMALL &&
		  !strcmp(BloodModProjectile_String(BMP_CTimeBomb, PK_ExplosionSound), "Sounds\\Weapons\\time\\explode.wav") &&
		  !strcmp(BloodModProjectile_String(BMP_CProximityBomb, PK_ExplosionSound), "Sounds\\Weapons\\proximities\\explode.wav") &&
		  !strcmp(BloodModProjectile_String(BMP_CRemoteBomb, PK_ExplosionSound), "Sounds\\Weapons\\time\\explode.wav") &&
		  BloodModProjectile_Float(BMP_CRemoteBomb, PK_ExplosionSoundRadius) == 1000.0f,
		  "and the bombs' explosions are their AddExplosion's literals, transcribed: SMALL/MEDIUM/SMALL, "
		  "time/proximities/time explode.wav, 1000");
#ifdef _ADD_ON
	Check(BloodModProjectile_Float(BMP_CGasGrenade, PK_SmokeTime) == GAS_GRENADE_SMOKE_TIME &&
		  BloodModProjectile_Float(BMP_CGasGrenade, PK_GasDamage) == GAS_GRENADE_MAX_DAMAGE &&
		  BloodModProjectile_Float(BMP_CGasGrenade, PK_GasDamageRadius) == GAS_GRENADE_DAMAGE_DIST &&
		  BloodModProjectile_Float(BMP_CGasGrenade, PK_GasDamageDelay) == GAS_GRENADE_DAMAGE_DELAY &&
		  BloodModProjectile_Int(BMP_CGasGrenade, PK_GasDamageType) == DAMAGE_TYPE_NORMAL &&
		  BloodModProjectile_Float(BMP_CFlayerPortal, PK_SearchRange) == FLAYER_ALT_SEARCH_RANGE &&
		  BloodModProjectile_Float(BMP_CFlayerPortal, PK_ChainInterval) == FLAYER_ALT_CHAIN_INTERVAL,
		  "and the add-on's: the gas grenade's and the flayer portal's");
#endif
}

void TestMembers()
{
	printf("projectiles: members\n");
	std::string err;
	int nBad = 0;
	for (int c = 0; c < BMP_COUNT; ++c)
	{
		const std::string text = std::string("[") + BloodModProjectile_Class(c) + "]\n"
			"DamageType = \"ACID\"\nLifeTime = 2.5\nGravity = TRUE\nTrailFX = \"SMOKETRAIL_1\"\nExplosion = FALSE\n";
		unsigned char nDamage = DAMAGE_TYPE_EXPLODE;
		float fLife = 10.0f;
		int nGravity = -1;
		unsigned int dwTrail = OBJFX_NONE, bExplosion = 1;
		const bool bLoaded = LoadProjectiles(text, err);
		const int n = BloodModProjectile_Apply(c, nDamage, fLife, nGravity, dwTrail, bExplosion);
		// And another class keeps its constructor's values.
		unsigned char nOther = DAMAGE_TYPE_EXPLODE;
		float fOther = 10.0f;
		int nGOther = -1;
		unsigned int dwTOther = OBJFX_NONE, bEOther = 1;
		const int nO = BloodModProjectile_Apply((c + 1) % BMP_COUNT, nOther, fOther, nGOther, dwTOther, bEOther);
		const bool bOk = bLoaded && n == 5 && nDamage == DAMAGE_TYPE_ACID && fLife == 2.5f && nGravity == 1 &&
						 dwTrail == OBJFX_SMOKETRAIL_1 && bExplosion == 0 &&
						 nO == 0 && nOther == DAMAGE_TYPE_EXPLODE && fOther == 10.0f && nGOther == -1 && bEOther == 1;
		if (!bOk && ++nBad <= 4)
			printf("        %s: loaded %d, %d keys, %s\n", BloodModProjectile_Class(c), (int)bLoaded, n, err.c_str());
	}
	char b[160];
	snprintf(b, sizeof(b), "each of the %d classes takes all five every-class keys, written over the constructor's, "
			 "and the next class keeps its own", (int)BMP_COUNT);
	Check(nBad == 0, b);
	Check(LoadProjectiles("[CGrenade]\nGravity = FALSE\nTrailFX = \"none\"\n", err), "Gravity FALSE and TrailFX NONE load");
	unsigned char nD = DAMAGE_TYPE_EXPLODE;
	float fL = 3.0f;
	int nG = -1;
	unsigned int dwT = OBJFX_SMOKETRAIL_2, bE = 1;
	BloodModProjectile_Apply(BMP_CGrenade, nD, fL, nG, dwT, bE);
	Check(nG == 0 && dwT == OBJFX_NONE && nD == DAMAGE_TYPE_EXPLODE && fL == 3.0f,
		  "and clear gravity and the trail, leaving the rest the constructor's");
	Check(BloodModProjectiles_Summary() == "CGrenade: Gravity FALSE, TrailFX \"NONE\"",
		  "the summary names what the mod set: " + BloodModProjectiles_Summary());
	Unload();
}

void TestClassKeys()
{
	printf("projectiles: class keys\n");
	std::string err;
	int nCases = 0, nBad = 0, nRefused = 0;
	for (int k = PK_BounceCount; k < PK_COUNT; ++k)
		for (int c = 0; c < BMP_COUNT; ++c)
		{
			const char* key = BloodModProjectile_Key(k);
			std::string v = "2.0";
			if (k == PK_BounceCount) v = "2";
			else if (k == PK_GasDamageType) v = "\"ACID\"";
			else if (k == PK_ExplosionType) v = "\"NAPALM_PRIMARY\"";
			else if (k == PK_ExplosionSound) v = "\"Sounds\\Weapons\\napalm\\altimpact.wav\"";
			const std::string text = std::string("[") + BloodModProjectile_Class(c) + "]\n" + key + " = " + v + "\n";
			++nCases;
			err.clear();
			const bool bLoaded = LoadProjectiles(text, err);
			bool bOk;
			if (BloodModProjectile_Has(c, k))
			{
				bOk = bLoaded && BloodModProjectile_Set(c, k);
				if (k == PK_BounceCount) bOk = bOk && BloodModProjectile_ModInt(c, k) == 2;
				else if (k == PK_GasDamageType) bOk = bOk && BloodModProjectile_Int(c, k) == DAMAGE_TYPE_ACID;
				else if (k == PK_ExplosionType) bOk = bOk && BloodModProjectile_Int(c, k) == EXP_NAPALM_PRIMARY;
				else if (k == PK_ExplosionSound)
					bOk = bOk && !strcmp(BloodModProjectile_String(c, k), "Sounds\\Weapons\\napalm\\altimpact.wav");
				else bOk = bOk && BloodModProjectile_Float(c, k) == 2.0f;
			}
			else
			{
				bOk = !bLoaded && err.find(std::string("]:") + key + ": only ") != std::string::npos;
				nRefused += bOk;
			}
			if (!bOk && ++nBad <= 6)
				printf("        [%s] %s = %s: loaded %d, %s\n", BloodModProjectile_Class(c), key, v.c_str(), (int)bLoaded,
					   err.c_str());
		}
	char b[200];
	snprintf(b, sizeof(b), "%d class-only overrides: each loads on the classes that have it, and the %d on any other "
			 "are refused, naming the classes that have it", nCases, nRefused);
	Check(nBad == 0, b);
	Unload();
}

void TestRoundTrip()
{
	printf("projectiles: round trip\n");
	std::string err;
	Unload();
	const bool bLoaded = LoadProjectiles(ProjectilesDump(), err);
	Check(bLoaded && Differ().empty(), "the dump loads as a mod and leaves every constant retail's" +
		  (err.empty() ? std::string() : ": " + err) + Join(Differ()));
	const std::string bent = std::string(ProjectilesDump()).replace(ProjectilesDump().find("BounceDist = 1500.0"),
																	 strlen("BounceDist = 1500.0"), "BounceDist = 3000.0");
	const bool bBent = LoadProjectiles(bent, err);
	const std::vector<std::string> d = Differ();
	Check(bBent && d.size() == 1 && d[0].compare(0, 30, "CDeathRayProjectile.BounceDist") == 0,
		  "calibration: a dump with the death ray's BounceDist moved is caught there and only there: " + Join(d));
	Unload();
}

void TestErrors()
{
	printf("projectiles: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a class nothing registers",		"[CRocket]\nLifeTime = 1.0\n",				"[CRocket] is not a Blood 2 projectile class" },
		{ "CProjectile itself",			"[CProjectile]\nLifeTime = 1.0\n",			"is not a Blood 2 projectile class" },
		{ "a key there is not",			"[CGrenade]\nSpeed = 1.0\n",					"has no key \"Speed\"" },
		{ "a damage type there is not",	"[CGrenade]\nDamageType = \"LASER\"\n",		"\"LASER\" is not a DAMAGE_TYPE_ name" },
		{ "an effect there is not",		"[CGrenade]\nTrailFX = \"SPARKLE\"\n",		"\"SPARKLE\" is not an OBJFX_ name" },
		{ "an explosion there is not",		"[CTimeBomb]\nExplosionType = \"HUGE\"\n",	"\"HUGE\" is not an EXP_ name" },
		{ "a negative lifetime",			"[CGrenade]\nLifeTime = -1.0\n",				"[CGrenade]:LifeTime is out of range" },
		{ "a negative bounce count",		"[CGrenade]\nBounceCount = -1\n",			"[CGrenade]:BounceCount is out of range" },
		{ "a TimeDelay on a grenade",		"[CGrenade]\nTimeDelay = 2.0\n",				"[CGrenade]:TimeDelay: only CTimeBomb has it" },
		{ "a number for a name",			"[CGrenade]\nDamageType = 5\n",				"is not a quoted string" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadProjectiles("[CDeathRayProjectile]\nBounceDist = 3000.0\n", err);
		const bool bBefore = Differ().size() == 1;
		err.clear();
		const bool bLoaded = LoadProjectiles(bad[i].text, err);
		Check(bBefore && !bLoaded && Differ().empty() && !BloodModProjectile_Set(BMP_CGrenade, PK_LifeTime) &&
			  err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	std::string err;
	Check(MountAndLoad(0, "", err, BloodModProjectiles_Load) && err.empty() && Differ().empty(),
		  "a mod without Projectiles.txt is no change and no error");
	Unload();
}

} // namespace

std::string ProjectilesDump()
{
	return
		"// Blood II's projectile classes and the constants a mod's\n"
		"// Mod\\Projectiles.txt can set for them. GENERATED by `testBlood2Tables\n"
		"// --dump projectiles` from Blood2/ObjectDLL/BloodModProjectiles.cpp and\n"
		"// ProjectileConstants.h; do not edit, regenerate. testBlood2Tables proves\n"
		"// that loading this file changes nothing.\n"
		"//\n"
		"// A section is a class, as a weapon's ProjectileClass names it.  Every\n"
		"// class takes DamageType (a DAMAGE_TYPE_ name without the prefix),\n"
		"// LifeTime, Gravity, TrailFX (an OBJFX_ name, or NONE) and Explosion, over\n"
		"// the values its constructor gives it, which are not listed here: the\n"
		"// classes are not a table.  With Mod\\AI.txt's [Options] Trace = TRUE the\n"
		"// server prints them, for each class the game makes.  BounceCount and\n"
		"// TimeDelay are listed under the classes that have them; the values\n"
		"// below are the class constants' retail ones.  A mod names only what it\n"
		"// changes:\n"
		"//\n"
		"//     [CGrenade]\n"
		"//     LifeTime = 2.0\n"
		"//\n"
		"// Blood2/ObjectDLL/BloodModProjectiles.h has the rest.\n"
		+ BloodModProjectiles_Reference();
}

void ProjectilesSelfTest()
{
	Unload();
	g_Retail = Constants();
	TestClasses();
	TestConstants();
	TestMembers();
	TestClassKeys();
	TestRoundTrip();
	TestErrors();
}
