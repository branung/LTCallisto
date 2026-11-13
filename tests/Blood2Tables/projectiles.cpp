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
	Check(listed.size() == 42 && listed.count("CGasGrenade") && listed.count("CFlayerPortal") && listed.count("CModProjectile"),
		  "42 with the Nightmare Levels, CGasGrenade, CFlayerPortal and CModProjectile among them");
#else
	Check(listed.size() == 37 && !listed.count("CGasGrenade") && listed.count("CModProjectile"),
		  "37 in the base game, CModProjectile (71.19) among them, without the add-on's CGasGrenade");
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

// The class-key sweep's value for key k, of its kind.
bool SweepIsBool(int k)
{
	return k == PK_BurstAfterBounces || k == PK_SpinStops || k == PK_BurstOnExpire || k == PK_BurstOnTouch ||
		   k == PK_ExplosionAlongNormal || k == PK_AttachToHead;
}
std::string SweepValue(int k)
{
	if (k == PK_BounceCount || k == PK_ChildCount) return "2";
	if (k == PK_GasDamageType) return "\"ACID\"";
	if (k == PK_ExplosionType) return "\"NAPALM_PRIMARY\"";
	if (k == PK_ExplosionSound || k == PK_LoopSound) return "\"Sounds\\Weapons\\napalm\\altimpact.wav\"";
	if (SweepIsBool(k)) return "TRUE";
	if (k == PK_LightColor) return "<0.5, 0.25, 1>";
	if (k == PK_Children) return "\"CGrenade\"";
	if (k == PK_BurstFX) return "\"FLAMING_2 FLAMING_3\"";
	if (k == PK_ChildSideSpeedMax || k == PK_ChildAngleMax) return "200.0";	// above its Min's retail value
	return "2.0";
}

// One case of the sweep: class c given key k. It is kept out of line, because with the cases
// inline GCC's loop unswitching at -O3 gave the keys that have a value of their own the wrong
// value.
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline))
#endif
bool SweepCase(int c, int k, int& nRefused)
{
	std::string err;
	const char* key = BloodModProjectile_Key(k);
	const bool bBool = SweepIsBool(k);
	const bool bPath = k == PK_ExplosionSound || k == PK_LoopSound;
	const std::string v = SweepValue(k);
	const std::string text = std::string("[") + BloodModProjectile_Class(c) + "]\n" + key + " = " + v + "\n";
	const bool bLoaded = LoadProjectiles(text, err);
	bool bOk;
	if (BloodModProjectile_Has(c, k))
	{
		bOk = bLoaded && BloodModProjectile_Set(c, k);
		float col[3];
		BloodModProjectile_Vector(c, k, col);
		int ids[4] = { -1, -1, -1, -1 };
		if (k == PK_BounceCount || k == PK_ChildCount) bOk = bOk && BloodModProjectile_ModInt(c, k) == 2;
		else if (k == PK_GasDamageType) bOk = bOk && BloodModProjectile_Int(c, k) == DAMAGE_TYPE_ACID;
		else if (k == PK_ExplosionType) bOk = bOk && BloodModProjectile_Int(c, k) == EXP_NAPALM_PRIMARY;
		else if (bPath)
			bOk = bOk && !strcmp(BloodModProjectile_String(c, k), "Sounds\\Weapons\\napalm\\altimpact.wav");
		else if (bBool) bOk = bOk && BloodModProjectile_Int(c, k) == 1;
		else if (k == PK_LightColor) bOk = bOk && col[0] == 0.5f && col[1] == 0.25f && col[2] == 1.0f;
		else if (k == PK_Children)
			bOk = bOk && !strcmp(BloodModProjectile_String(c, k), "CGrenade") &&
				  BloodModProjectile_Int(c, k) == BMP_CGrenade;
		else if (k == PK_ChildSideSpeedMax || k == PK_ChildAngleMax) bOk = bOk && BloodModProjectile_Float(c, k) == 200.0f;
		else if (k == PK_BurstFX)
			bOk = bOk && BloodModProjectile_FXList(c, k, ids, 4) == 2 && ids[0] == OBJFX_FLAMING_2 && ids[1] == OBJFX_FLAMING_3;
		else bOk = bOk && BloodModProjectile_Float(c, k) == 2.0f;
	}
	else
	{
		bOk = !bLoaded && err.find(std::string("]:") + key + ": only ") != std::string::npos;
		nRefused += bOk;
	}
	if (!bOk)
		printf("        [%s] %s = %s: loaded %d, %s\n", BloodModProjectile_Class(c), key, v.c_str(), (int)bLoaded,
			   err.c_str());
	return bOk;
}

void TestClassKeys()
{
	printf("projectiles: class keys\n");
	int nCases = 0, nBad = 0, nRefused = 0;
	for (int k = PK_BounceCount; k < PK_COUNT; ++k)
		for (int c = 0; c < BMP_COUNT; ++c)
		{
			++nCases;
			if (!SweepCase(c, k, nRefused) && ++nBad > 6)
				break;
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

// Mod\Projectiles.txt's new classes: a Base, its keys and values
void TestNewClasses()
{
	printf("projectiles: new classes\n");
	std::string err;
	Unload();
	const bool bLoaded = LoadProjectiles(
		"[CGrenade]\nBounceCount = 2\nBurstAfterBounces = TRUE\nSpinStops = FALSE\n"
		"[CJugGrenade]\nBase = \"CGrenade\"\nLifeTime = 1.0\n", err);
	const int nJug = BloodModProjectile_FindClass("cjuggrenade");
	unsigned char nD = DAMAGE_TYPE_EXPLODE;
	float fL = 3.0f;
	int nG = -1;
	unsigned int dwT = OBJFX_NONE, bE = 1;
	const int nKeys = BloodModProjectile_Apply(nJug, nD, fL, nG, dwT, bE);
	Check(bLoaded && nJug == BMP_COUNT && BloodModProjectile_ClassCount() == BMP_COUNT + 1 &&
		  BloodModProjectile_NewCount() == 1 && BloodModProjectile_Base(nJug) == BMP_CGrenade &&
		  BloodModProjectile_Root(nJug) == BMP_CGrenade && !strcmp(BloodModProjectile_Class(nJug), "CJugGrenade"),
		  "a section with a Base is a new class, after retail's, built on its Base: " + err);
	Check(nKeys == 1 && fL == 1.0f && BloodModProjectile_ModInt(nJug, PK_BounceCount) == 2 &&
		  BloodModProjectile_Int(nJug, PK_BurstAfterBounces) == 1 && BloodModProjectile_Int(nJug, PK_SpinStops) == 0 &&
		  BloodModProjectile_Int(BMP_CTimeBomb, PK_BounceCount) == 0,
		  "it takes the values the file gives its Base, then its own; another class keeps retail's");
	Check(BloodModProjectile_Has(nJug, PK_BounceCount) && !BloodModProjectile_Has(nJug, PK_TimeDelay),
		  "and has its Base's keys and no others");
	Check(BloodModProjectiles_Summary() ==
		  "CGrenade: BounceCount 2, BurstAfterBounces TRUE, SpinStops FALSE\nCJugGrenade: Base \"CGrenade\", LifeTime 1.0",
		  "the summary names each class's own values and a new class's Base: " + BloodModProjectiles_Summary());
	Unload();
	Check(BloodModProjectile_ClassCount() == BMP_COUNT && BloodModProjectile_FindClass("CJugGrenade") < 0,
		  "unloading forgets the new classes");

	// A chain, its Base given after it, on the class made to be a Base.
	const bool bChain = LoadProjectiles(
		"[CShellFrag]\nBase = \"CShell\"\nChildCount = 0\n"
		"[CShell]\nBase = \"CModProjectile\"\nModel = \"Models\\Ammo\\Howitzer_Round.abc\"\nChildren = \"CShellFrag\"\n"
		"ChildCount = 8\nLightColor = <1, 0.75, 0>\nExplosionType = \"HOWITZER_ALT\"\n", err);
	const int nFrag = BloodModProjectile_FindClass("CShellFrag"), nShell = BloodModProjectile_FindClass("CShell");
	float col[3];
	BloodModProjectile_Vector(nFrag, PK_LightColor, col);
	Check(bChain && BloodModProjectile_Root(nFrag) == BMP_CModProjectile && BloodModProjectile_Base(nFrag) == nShell &&
		  BloodModProjectile_Int(nShell, PK_ChildCount) == 8 && BloodModProjectile_Int(nFrag, PK_ChildCount) == 0 &&
		  BloodModProjectile_Int(nShell, PK_Children) == nFrag &&
		  !strcmp(BloodModProjectile_ModString(nFrag, PK_Model), "Models\\Ammo\\Howitzer_Round.abc") &&
		  col[0] == 1.0f && col[1] == 0.75f && col[2] == 0.0f &&
		  BloodModProjectile_Int(nFrag, PK_ExplosionType) == EXP_HOWITZER_ALT &&
		  BloodModProjectile_Float(nFrag, PK_ChildSpeed) == MODPROJ_CHILD_SPEED,
		  "a Base may be a new class given later in the file, and a class's Children may be a new one: " + err);
	Unload();

	// The look and flag keys every class takes
	const bool bLook = LoadProjectiles("[CGrenade]\nSprite = TRUE\nModel = \"Sprites\\napalm.spr\"\nScale = 0.25\n"
									   "AppearDelay = 0.01\nPointCollide = TRUE\nNoSliding = FALSE\nClientFX = FALSE\n", err);
	Check(bLook && BloodModProjectile_ModInt(BMP_CGrenade, PK_Sprite) == 1 &&
		  BloodModProjectile_ModFloat(BMP_CGrenade, PK_Scale) == 0.25f &&
		  BloodModProjectile_ModFloat(BMP_CGrenade, PK_AppearDelay) == 0.01f &&
		  BloodModProjectile_Set(BMP_CGrenade, PK_NoSliding) && BloodModProjectile_ModInt(BMP_CGrenade, PK_NoSliding) == 0 &&
		  BloodModProjectile_Set(BMP_CTeslaProjectile, PK_Sprite) == false,
		  "every class takes Sprite, Scale, AppearDelay, PointCollide, NoSliding and ClientFX: " + err);
	Unload();

	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a new class without a Base",		"[CRocket]\nLifeTime = 1.0\n",	"and a new class needs a Base" },
		{ "a Base on a retail class",			"[CGrenade]\nBase = \"CTimeBomb\"\n",	"[CGrenade]:Base: CGrenade is a retail class" },
		{ "a Base there is not",				"[CRocket]\nBase = \"CMissile\"\n",	"\"CMissile\" is not a projectile class" },
		{ "Bases in a ring",					"[CA]\nBase = \"CB\"\n[CB]\nBase = \"CA\"\n",	"its Bases come back to it" },
		{ "a key its Base has not",			"[CRocket]\nBase = \"CGrenade\"\nTimeDelay = 1.0\n",
												"only CTimeBomb has it, and a new class has its Base's keys" },
		{ "Children there is not",				"[CRocket]\nBase = \"CModProjectile\"\nChildren = \"CNone\"\n",
												"[CRocket]:Children: \"CNone\" is not a projectile class" },
		{ "too many children",					"[CRocket]\nBase = \"CModProjectile\"\nChildCount = 65\n",	"ChildCount is out of range" },
		{ "a colour above 1",					"[CRocket]\nBase = \"CModProjectile\"\nLightColor = <2, 0, 0>\n",	"LightColor is out of range" },
		{ "a min above its max",				"[CRocket]\nBase = \"CModProjectile\"\nChildAngleMin = 160\n",
												"[CRocket]: ChildAngleMin is above ChildAngleMax" },
		{ "an angle past a turn",				"[CModProjectile]\nChildAngleMax = 400\n",	"ChildAngleMax is out of range" },
#ifdef _ADD_ON
		{ "a BurstFX there is not",			"[CGasGrenade]\nBurstFX = \"FLAMING_2 SPARKLE\"\n",
												"[CGasGrenade]:BurstFX: \"SPARKLE\" is not an OBJFX_ name" },
#endif
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		err.clear();
		const bool bBad = LoadProjectiles(bad[i].text, err);
		Check(!bBad && BloodModProjectile_ClassCount() == BMP_COUNT && Differ().empty() &&
			  err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing: " + err);
	}
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
		"// A section named after no class is a new one, with Base = the class\n"
		"// it is built on, retail's or the file's; it has that class's keys.\n"
		"// CModProjectile is a class made to be a Base, every value a key.\n"
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
	TestNewClasses();
}
