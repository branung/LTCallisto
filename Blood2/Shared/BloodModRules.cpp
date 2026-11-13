// BloodModRules.cpp: see BloodModRules.h

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"			// the OBJFX_ names
#include "BloodModRules.h"
#include "BloodModCharacters.h"	// the item and ammo lists' grammar
#include "BloodModTable.h"		// PrintWrapped, DF, kMaxString

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <string>
#include <vector>

// Retail's values, each from the code that held it
BloodModRulesValues gRules =
{
	0.08f,			// PlayerObj.cpp's ALPHA_INVISIBLE
	82.0f,			// PlayerObj.cpp's DIST_USE
	60.0f,			// PlayerObj.cpp's MAX_IDLE_TIME
	"",				// CPlayerObj::InitialUpdate gives no item
	"BULLET 50",	// CPlayerObj::InitialUpdate's AddAmmo(AMMO_BULLET, 50)
	24.0f, 16.0f, 32.0f, 80.0f,	// BloodClientShell.cpp's BOBV_, BOBH_, SWAYV_, SWAYH_WALKING
	3.0f,			// CBloodClientShell::UpdateScreenShake's * 3.0f
	1,				// StartWonkyVision starts it
	20.0f, 60.0f,	// CViewWeapon's m_fMinIdleDelay, m_fMaxIdleDelay
	1,				// UpdateWeapon: "If we have duel weapons, make the altfire the same as the primary"
	0.2f,			// MoveMgr.cpp's LADDER_STOP_TIME
	0,				// m_bCanSwimJump: FALSE at start and after a jump
	1,				// MoveMgr's m_bSwimmingOnSurface = DTRUE on breaking the surface
	120.0f,			// GameInvItems.h's FLASHLIGHTCHARGETIME
	30.0f,			// GameInvItems.h's GOGGLESCHARGETIME
	0.40f,			// FlashlightFX.cpp's fDistance * 0.40f
	25.0f,			// CWeapon::UpdateWeaponFX's fDamage > 25.0f
	// WeaponFX.cpp's:
	0.05f, 0.15f,	// CreateBloodSplat's 0.1f + GetRandom(-0.05f,0.05f)
	1, 1,			// CreateBloodSpurt's nNumEmitters = 1
	2.0f, 0.5f,		// CreateSparks' metal fDuration, big and small
	5.0f, 200.0f,	// its flesh fEmissionRadius and fRadius
	1, 3,			// CreateFragments' GetRandom(1,3), wood and stone
	1,				// CreateSmoke's bubbles in place of smoke under liquid
	0.0f, 0.35f,	// its fDelay and vMaxScale
	50.0f,			// CreateBubbles' fGravity
	"",				// GetMarkSprite: liquid falls to the stone's marks
	"FLAMING_2",	// CCorpse::Setup's burning corpse
	"SMOKING_2",	// CCorpse::Update's smoking corpse
	0,				// CCorpse::HandleDamage gibs on its own damage
	100,			// CCorpse::CreateGibs: "100 % create a head"
	80,				// its IsRandomChance(80) for each arm
	60,				// and IsRandomChance(60) for each leg
	// SFXMgr.cpp's:
	20, 2, 1,		// CreateGibs: IsRandomChance(20) *= 2, else *= (DBYTE)1.5
	{ 300, 300, 80, 80, 80, 80, 100, 80 },	// s_nDynArrayMaxNums
	0,				// CNewStatusBar::Reset's m_nWeapLevel = 0, WEAPTAB_INVISIBLE
	"",				// BloodClientShell.h's version_string
	1,				// CMenuCustomLevel adds Worlds\Multi and Worlds_ao\Multi_ao
	0,				// the add-on goes from the GTI logo to MOVIE_FINISHED
	// CPlayerObj::HandleCheatCode's:
	0,				// CHEAT_KFA: SetFullAmmo only
	"BERETTA FLAREGUN HOWITZER SINGULARITY BUGSPRAY SNIPERRIFLE NAPALMCANNON MINIGUN DEATHRAY VOODOO ORB",	// CHEAT_KFA2
	"MEDKIT FLASHLIGHT NIGHTGOGGLES THEEYE BINOCULARS",	// CHEAT_GIVEALLINV
	// PlayerObj.cpp's:
	0,				// VME_POWERUP when invulnerability ends (CheckPowerups)
	1,				// VME_SUICIDE for a death by one's own hand
};

namespace
{

const char* const kPath = "Mod\\Rules.txt";

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

// RK_INT is an int up to fMax; RK_PATH any string, "" too. fMax caps a float as well (0: none).
enum Kind { RK_FLOAT, RK_PERCENT, RK_BOOL, RK_ITEMS, RK_AMMO, RK_OBJFX, RK_INT, RK_PATH, RK_WEAPONS };
struct Rule { const char* pSection; const char* pKey; Kind eKind; size_t nOffset; float fMax; };
#define R(sec, key, kind, member) { sec, key, RK_##kind, offsetof(BloodModRulesValues, member), 0.0f }
#define RM(sec, key, kind, member, mx) { sec, key, RK_##kind, offsetof(BloodModRulesValues, member), mx }
const Rule kRules[] =
{
	RM("Player", "InvisibleAlpha",		FLOAT,		fInvisibleAlpha, 1.0f),
	R("Player", "UseDistance",			FLOAT,		fUseDistance),
	R("Player", "IdleVoiceTime",		FLOAT,		fIdleVoiceTime),
	R("Player", "SpawnItems",			ITEMS,		szSpawnItems),
	R("Player", "SpawnAmmo",			AMMO,		szSpawnAmmo),
	R("Camera", "BobVertical",			FLOAT,		fBobVertical),
	R("Camera", "BobHorizontal",		FLOAT,		fBobHorizontal),
	R("Camera", "SwayVertical",			FLOAT,		fSwayVertical),
	R("Camera", "SwayHorizontal",		FLOAT,		fSwayHorizontal),
	R("Camera", "ShakeScale",			FLOAT,		fShakeScale),
	R("Camera", "WonkyVision",			BOOL,		bWonkyVision),
	R("ViewWeapon", "IdleDelayMin",		FLOAT,		fIdleDelayMin),
	R("ViewWeapon", "IdleDelayMax",		FLOAT,		fIdleDelayMax),
	R("ViewWeapon", "DualAltAsPrimary",	BOOL,		bDualAltAsPrimary),
	R("Movement", "LadderStopTime",		FLOAT,		fLadderStopTime),		// above 0: it divides
	R("Movement", "SwimJumpRepeat",		BOOL,		bSwimJumpRepeat),
	R("Movement", "SurfaceFloat",		BOOL,		bSurfaceFloat),
	R("Items", "FlashlightChargeTime",	FLOAT,		fFlashlightChargeTime),
	R("Items", "GogglesChargeTime",		FLOAT,		fGogglesChargeTime),
	R("Items", "FlashlightRadius",		FLOAT,		fFlashlightRadius),
	R("Impacts", "BloodSpurtDamage",	FLOAT,		fBloodSpurtDamage),
	R("Impacts", "SplatScaleMin",		FLOAT,		fSplatScaleMin),
	R("Impacts", "SplatScaleMax",		FLOAT,		fSplatScaleMax),
	RM("Impacts", "SpurtEmittersMin",	INT,		nSpurtEmittersMin, 16),
	RM("Impacts", "SpurtEmittersMax",	INT,		nSpurtEmittersMax, 16),
	R("Impacts", "BigMetalSparkTime",	FLOAT,		fBigMetalSparkTime),
	R("Impacts", "MetalSparkTime",		FLOAT,		fMetalSparkTime),
	R("Impacts", "BloodSparkEmissionRadius", FLOAT,	fBloodSparkEmissionRadius),
	R("Impacts", "BloodSparkRadius",	FLOAT,		fBloodSparkRadius),
	RM("Impacts", "FragmentsMin",		INT,		nFragmentsMin, 100),		// a char loop count
	RM("Impacts", "FragmentsMax",		INT,		nFragmentsMax, 100),
	R("Impacts", "BubblesInLiquid",		BOOL,		bBubblesInLiquid),
	R("Impacts", "SmokeDelay",			FLOAT,		fSmokeDelay),
	R("Impacts", "SmokeScale",			FLOAT,		fSmokeScale),
	R("Impacts", "BubbleGravity",		FLOAT,		fBubbleGravity),
	R("Impacts", "LiquidMark",			PATH,		szLiquidMark),
	R("Corpse", "BurnFX",				OBJFX,		szBurnFX),
	R("Corpse", "SmokeFX",				OBJFX,		szSmokeFX),
	R("Corpse", "GibOnExplosionOnly",	BOOL,		bGibOnExplosionOnly),
	R("Corpse", "GibHeadChance",		PERCENT,	nGibHeadChance),
	R("Corpse", "GibArmChance",			PERCENT,	nGibArmChance),
	R("Corpse", "GibLegChance",			PERCENT,	nGibLegChance),
	R("Effects", "FleshGibChance",		PERCENT,	nFleshGibChance),
	RM("Effects", "FleshGibMore",		INT,		nFleshGibMore, 16),		// a DBYTE count's multiplier
	RM("Effects", "FleshGibScale",		INT,		nFleshGibScale, 16),
	RM("Effects", "GeneralCap",			INT,		nCaps[0], 4096),
	RM("Effects", "MarkCap",			INT,		nCaps[1], 4096),
	RM("Effects", "WeaponCap",			INT,		nCaps[2], 4096),
	RM("Effects", "SmokeCap",			INT,		nCaps[3], 4096),
	RM("Effects", "ExplosionCap",		INT,		nCaps[4], 4096),
	RM("Effects", "GibCap",				INT,		nCaps[5], 4096),
	RM("Effects", "CasingCap",			INT,		nCaps[6], 4096),
	RM("Effects", "ObjectCap",			INT,		nCaps[7], 4096),
	R("HUD", "WeaponTabsOnReset",		BOOL,		bWeaponTabsOnReset),
	R("Menus", "VersionText",			PATH,		szVersionText),
	R("Menus", "CustomLevelsMultiplayer", BOOL,		bCustomLevelsMultiplayer),
	R("Menus", "AddonIntroMovie",		BOOL,		bAddonIntroMovie),
	R("Cheats", "InfiniteAmmo",			BOOL,		bCheatInfiniteAmmo),
	R("Cheats", "MoreWeapons",			WEAPONS,	szCheatMoreWeapons),
	R("Cheats", "AllItems",				ITEMS,		szCheatAllItems),
	R("Voice", "WillpowerVoiceAtPickup", BOOL,		bWillpowerVoiceAtPickup),
	R("Voice", "SuicideVoice",			BOOL,		bSuicideVoice),
};
#undef RM

// Each minimum and its maximum, a random value between which must not run backwards (GetRandom)
const char* const kPairs[][2] =
{
	{ "IdleDelayMin", "IdleDelayMax" }, { "SplatScaleMin", "SplatScaleMax" },
	{ "SpurtEmittersMin", "SpurtEmittersMax" }, { "FragmentsMin", "FragmentsMax" },
};
#undef R
const int kNumRules = (int)(sizeof(kRules) / sizeof(kRules[0]));

// SFXMsgIds.h's OBJFX_ names, without the prefix; the add-on's two only with _ADD_ON, as the
// header has them. Mod\AI.txt's death effects and the projectile and weapon classes' keys read
// the same list (BloodModAI.h).
struct ObjFX { const char* pName; int nId; };
#define E(n) { #n, OBJFX_##n },
const ObjFX kObjFX[] =
{
	E(SMOKETRAIL_1) E(SMOKETRAIL_2) E(FIRETRAIL_1) E(FIRETRAIL_2) E(FIRETRAIL_3) E(SPARKS_1) E(SPARKS_2) E(SPARKS_3)
	E(SINGULARITY_1) E(SINGULARITY_2) E(SINGULARITY_3) E(VOODOO_1) E(VOODOO_2) E(VOODOO_3) E(VOODOO_4) E(VOODOO_5)
	E(VOODOO_6) E(HEAL_1) E(SHIELD_1) E(SMOKING_1) E(SMOKING_2) E(SMOKING_3) E(FLAMING_1) E(FLAMING_2) E(FLAMING_3)
	E(GROUNDFLAME_1) E(GROUNDFLAME_2) E(STAFFTRAIL_1) E(STAFFTRAIL_2) E(STAFFTRAIL_3) E(BUGSPRAY_1) E(BUGSPRAY_2)
	E(ORBTRAIL_1) E(ORBTRAIL_2) E(ENERGYWALL_1) E(POWERUP_1) E(DEATHRAY_RING_1) E(DEATHRAY_RING_2) E(ELECTRIC_1)
#ifdef _ADD_ON
	E(GREEN_SMOKE) E(BLOOD_TRAIL_1)
#endif
};
#undef E
const int kNumObjFX = (int)(sizeof(kObjFX) / sizeof(kObjFX[0]));

BloodModRulesValues g_Retail;
bool g_bRetail = false;
std::deque<std::string> g_Strings;	// every string a mod gave: gRules keeps the pointers

void TakeRetail()
{
	if (!g_bRetail) { g_Retail = gRules; g_bRetail = true; }
}

float& F(BloodModRulesValues& v, int i)			{ return *(float*)((char*)&v + kRules[i].nOffset); }
int& N(BloodModRulesValues& v, int i)			{ return *(int*)((char*)&v + kRules[i].nOffset); }
const char*& S(BloodModRulesValues& v, int i)	{ return *(const char**)((char*)&v + kRules[i].nOffset); }

std::string ValueOf(BloodModRulesValues& v, int i)
{
	switch (kRules[i].eKind)
	{
		case RK_FLOAT:		return BloodMod::DF(F(v, i));
		case RK_PERCENT:
		case RK_INT:		{ char b[16]; snprintf(b, sizeof(b), "%d", N(v, i)); return b; }
		case RK_BOOL:		return N(v, i) ? "TRUE" : "FALSE";
		default:			return std::string("\"") + (S(v, i) ? S(v, i) : "") + "\"";
	}
}

// Whether a list key's text is good, and why not.
bool CheckList(Kind k, const char* p, std::string& why)
{
	if (k == RK_ITEMS) { std::vector<int> v; return BloodMod_ParseItems(p, v, &why); }
	if (k == RK_AMMO) { std::vector<BloodModAmmo> v; return BloodMod_ParseAmmo(p, v, &why); }
	if (k == RK_WEAPONS) { std::vector<int> v; return BloodMod_ParseWeapons(p, v, &why); }
	std::vector<int> v;
	if (!BloodModRules_ObjFXList(p, v, &why)) return false;
	if (v.empty()) { why = "it names no effect (one or more OBJFX_ names without the prefix)"; return false; }
	return true;
}

} // namespace

const char* BloodModRules_Key(int nKey)		{ return nKey >= 0 && nKey < kNumRules ? kRules[nKey].pKey : 0; }
const char* BloodModRules_Section(int nKey)	{ return nKey >= 0 && nKey < kNumRules ? kRules[nKey].pSection : 0; }

std::string BloodModRules_Value(int nKey, bool bRetail)
{
	if (nKey < 0 || nKey >= kNumRules) return "";
	TakeRetail();
	return ValueOf(bRetail ? g_Retail : gRules, nKey);
}

int BloodMod_ObjFX(const char* pName)
{
	if (!pName || !pName[0]) return -1;
	for (int i = 0; i < kNumObjFX; ++i)
		if (EqualNoCase(kObjFX[i].pName, pName)) return kObjFX[i].nId;
	return -1;
}

const char* BloodMod_ObjFXName(int nId)
{
	for (int i = 0; i < kNumObjFX; ++i)
		if (kObjFX[i].nId == nId) return kObjFX[i].pName;
	return 0;
}

bool BloodModRules_ObjFXList(const char* pList, std::vector<int>& out, std::string* pWhy)
{
	out.clear();
	std::string w;
	for (const char* p = pList ? pList : ""; ; ++p)
	{
		if (!*p || *p == ' ' || *p == '\t' || *p == ',')
		{
			if (!w.empty())
			{
				const int n = BloodMod_ObjFX(w.c_str());
				if (n < 0)
				{
					if (pWhy) *pWhy = "\"" + w + "\" is not an OBJFX_ name without the prefix";
					out.clear();
					return false;
				}
				out.push_back(n);
			}
			w.clear();
			if (!*p) break;
		}
		else
			w += *p;
	}
	return true;
}

void BloodModRules_Clear()
{
	TakeRetail();
	gRules = g_Retail;
}

bool BloodModRules_Load(LT1Mod::FileSource &src, std::string &sError)
{
	BloodModRules_Clear();
	sError.clear();
	if (!LT1Mod::IsActive())
		return true;

	std::string text;
	if (!src.ReadFile(kPath, text))
		return true;

	LT1Mod::Attributes a;
	if (!a.Parse(text.data(), text.size(), kPath))
	{
		sError = a.Error();
		return false;
	}

	BloodModRulesValues v = g_Retail;
	std::vector<std::string> strings;		// kept only once the whole file is good
	std::vector<int> stringKeys;
	char buf[512];
	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		bool bSection = false;
		for (int i = 0; i < kNumRules && !bSection; ++i) bSection = EqualNoCase(kRules[i].pSection, tag);
		if (!bSection)
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is not a section (Player, Camera, ViewWeapon, Movement, Items, Impacts, Corpse, Effects, HUD, Menus; "
					 "Blood2/ModReference/Rules.txt lists them)", kPath, tag);
			sError = buf;
			return false;
		}
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			int r = -1;
			for (int i = 0; i < kNumRules && r < 0; ++i)
				if (EqualNoCase(kRules[i].pSection, tag) && EqualNoCase(kRules[i].pKey, key)) r = i;
			if (r < 0)
			{
				const char* pElse = 0;
				for (int i = 0; i < kNumRules && !pElse; ++i)
					if (EqualNoCase(kRules[i].pKey, key)) pElse = kRules[i].pSection;
				if (pElse)
					snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\": it is [%s]'s", kPath, tag, key, pElse);
				else
					snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\" (Blood2/ModReference/Rules.txt lists "
							 "them)", kPath, tag, key);
				sError = buf;
				return false;
			}
			const std::string before = a.Error();
			bool bRange = true;
			std::string why;
			const Kind kind = kRules[r].eKind;
			if (kind == RK_FLOAT)
			{
				F(v, r) = a.GetFloat(tag, key, 0.0f);
				bRange = F(v, r) >= 0.0f && F(v, r) <= (kRules[r].fMax > 0.0f ? kRules[r].fMax : 1e30f) &&
						 (F(v, r) > 0.0f || strcmp(kRules[r].pKey, "LadderStopTime"));	// it divides
			}
			else if (kind == RK_PERCENT || kind == RK_INT)
			{
				N(v, r) = a.GetInt(tag, key, 0);
				bRange = N(v, r) >= 0 && N(v, r) <= (kind == RK_PERCENT ? 100 : (int)kRules[r].fMax);
			}
			else if (kind == RK_BOOL)
				N(v, r) = a.GetBool(tag, key, false) ? 1 : 0;
			else
			{
				const std::string s = a.GetString(tag, key, "");
				if (a.Error() == before &&
					s.size() > (kind == RK_WEAPONS || kind == RK_ITEMS ? BloodMod::Table::kMaxList : BloodMod::Table::kMaxString))
					bRange = false;
				else if (a.Error() == before && kind != RK_PATH && !CheckList(kind, s.c_str(), why))
				{
					snprintf(buf, sizeof(buf), "%s: [%s]:%s: %s", kPath, tag, key, why.c_str());
					sError = buf;
					return false;
				}
				strings.push_back(s);
				stringKeys.push_back(r);
			}
			if (a.Error() != before)
			{
				sError = std::string(kPath) + ": " + a.Error();
				return false;
			}
			if (!bRange)
			{
				snprintf(buf, sizeof(buf), "%s: [%s]:%s is out of range", kPath, tag, key);
				sError = buf;
				return false;
			}
		}
	}
	// A minimum above its maximum would run GetRandom backwards.
	for (size_t p = 0; p < sizeof(kPairs) / sizeof(kPairs[0]); ++p)
	{
		int lo = -1, hi = -1;
		for (int i = 0; i < kNumRules; ++i)
		{
			if (!strcmp(kRules[i].pKey, kPairs[p][0])) lo = i;
			if (!strcmp(kRules[i].pKey, kPairs[p][1])) hi = i;
		}
		const bool bInt = kRules[lo].eKind == RK_INT;
		if (bInt ? N(v, lo) > N(v, hi) : F(v, lo) > F(v, hi))
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:%s is above %s", kPath, kRules[lo].pSection, kPairs[p][0], kPairs[p][1]);
			sError = buf;
			return false;
		}
	}
	for (size_t i = 0; i < strings.size(); ++i)
	{
		g_Strings.push_back(strings[i]);
		S(v, stringKeys[i]) = g_Strings.back().c_str();
	}
	gRules = v;
	return true;
}

std::string BloodModRules_Summary()
{
	TakeRetail();
	std::string out, section, line;
	for (int i = 0; i <= kNumRules; ++i)
	{
		if (i == kNumRules || (i && kRules[i].pSection != kRules[i - 1].pSection))
		{
			if (!line.empty()) out += std::string(out.empty() ? "" : "\n") + section + ": " + line;
			line.clear();
			if (i == kNumRules) break;
		}
		section = kRules[i].pSection;
		const std::string now = ValueOf(gRules, i);
		if (now != ValueOf(g_Retail, i))
			line += std::string(line.empty() ? "" : ", ") + kRules[i].pKey + " " + now;
	}
	return out;
}

std::string BloodModRules_Reference()
{
	TakeRetail();
	std::string out;
	for (int i = 0; i < kNumRules; ++i)
	{
		if (!i || kRules[i].pSection != kRules[i - 1].pSection)
			out += std::string("\n[") + kRules[i].pSection + "]\n";
		out += std::string(kRules[i].pKey) + " = " + ValueOf(g_Retail, i) + "\n";
	}
	return out;
}

void BloodModRules_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModRules_Load(src, sErr);
	if (!pPrint) return;
	BloodMod::PrintWrapped(bOk ? BloodModRules_Summary() : sErr,
						   std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(kPath) + ": " : std::string()), pPrint);
}
