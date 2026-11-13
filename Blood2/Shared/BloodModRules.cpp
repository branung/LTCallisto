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
	120.0f,			// GameInvItems.h's FLASHLIGHTCHARGETIME
	30.0f,			// GameInvItems.h's GOGGLESCHARGETIME
	25.0f,			// CWeapon::UpdateWeaponFX's fDamage > 25.0f
	"FLAMING_2",	// CCorpse::Setup's burning corpse
	"SMOKING_2",	// CCorpse::Update's smoking corpse
	0,				// CCorpse::HandleDamage gibs on its own damage
	100,			// CCorpse::CreateGibs: "100 % create a head"
	80,				// its IsRandomChance(80) for each arm
	60,				// and IsRandomChance(60) for each leg
};

namespace
{

const char* const kPath = "Mod\\Rules.txt";

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

enum Kind { RK_FLOAT, RK_PERCENT, RK_BOOL, RK_ITEMS, RK_AMMO, RK_OBJFX };
struct Rule { const char* pSection; const char* pKey; Kind eKind; size_t nOffset; };
#define R(sec, key, kind, member) { sec, key, RK_##kind, offsetof(BloodModRulesValues, member) }
const Rule kRules[] =
{
	R("Player", "InvisibleAlpha",		FLOAT,		fInvisibleAlpha),
	R("Player", "UseDistance",			FLOAT,		fUseDistance),
	R("Player", "IdleVoiceTime",		FLOAT,		fIdleVoiceTime),
	R("Player", "SpawnItems",			ITEMS,		szSpawnItems),
	R("Player", "SpawnAmmo",			AMMO,		szSpawnAmmo),
	R("Items", "FlashlightChargeTime",	FLOAT,		fFlashlightChargeTime),
	R("Items", "GogglesChargeTime",		FLOAT,		fGogglesChargeTime),
	R("Impacts", "BloodSpurtDamage",	FLOAT,		fBloodSpurtDamage),
	R("Corpse", "BurnFX",				OBJFX,		szBurnFX),
	R("Corpse", "SmokeFX",				OBJFX,		szSmokeFX),
	R("Corpse", "GibOnExplosionOnly",	BOOL,		bGibOnExplosionOnly),
	R("Corpse", "GibHeadChance",		PERCENT,	nGibHeadChance),
	R("Corpse", "GibArmChance",			PERCENT,	nGibArmChance),
	R("Corpse", "GibLegChance",			PERCENT,	nGibLegChance),
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
		case RK_PERCENT:	{ char b[16]; snprintf(b, sizeof(b), "%d", N(v, i)); return b; }
		case RK_BOOL:		return N(v, i) ? "TRUE" : "FALSE";
		default:			return std::string("\"") + (S(v, i) ? S(v, i) : "") + "\"";
	}
}

// Whether a list key's text is good, and why not.
bool CheckList(Kind k, const char* p, std::string& why)
{
	if (k == RK_ITEMS) { std::vector<int> v; return BloodMod_ParseItems(p, v, &why); }
	if (k == RK_AMMO) { std::vector<BloodModAmmo> v; return BloodMod_ParseAmmo(p, v, &why); }
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
			snprintf(buf, sizeof(buf), "%s: [%s] is not a section (Player, Items, Impacts, Corpse; "
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
				bRange = F(v, r) >= 0.0f && F(v, r) <= (kRules[r].nOffset == offsetof(BloodModRulesValues, fInvisibleAlpha)
															? 1.0f : 1e30f);		// an alpha is 0-1
			}
			else if (kind == RK_PERCENT)
			{
				N(v, r) = a.GetInt(tag, key, 0);
				bRange = N(v, r) >= 0 && N(v, r) <= 100;
			}
			else if (kind == RK_BOOL)
				N(v, r) = a.GetBool(tag, key, false) ? 1 : 0;
			else
			{
				const std::string s = a.GetString(tag, key, "");
				if (a.Error() == before && s.size() > BloodMod::Table::kMaxString)
					bRange = false;
				else if (a.Error() == before && !CheckList(kind, s.c_str(), why))
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
