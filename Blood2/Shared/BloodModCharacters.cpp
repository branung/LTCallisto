#include "server_de.h"
#include "SharedDefs.h"
#include "CharacterValues.h"
#include "BloodModCharacters.h"
#include "BloodModTable.h"
#include "BloodModWeapons.h"
#include "lt1mod_registry.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace BloodMod;

namespace
{

// The CHARACTER_ names, by id (SharedDefs.h). Every id below MAX_CHARACTERS has exactly one.
#define C(x) { CHARACTER_##x, #x }
const Entry kCharacterNames[] =
{
	C(CALEB), C(OPHELIA), C(ISHMAEL), C(GABREILLA),
#ifdef _ADD_ON
	C(M_CULTIST), C(F_CULTIST), C(SOULDRUDGE), C(PROPHET),
#endif
};
#undef C
const int kNumNames = (int)(sizeof(kCharacterNames) / sizeof(kCharacterNames[0]));

// Array 0 is gCharacterValues, array 1 is gCharacterSkins (a row of names per character),
// array 2 is gCharacterPictures (the menu's).
#define F(key, member, kind) { key, 0, offsetof(CharacterValues, member), kind, 0 }
#define S(key, skin) { key, 1, (skin) * sizeof(char*), K_STRING, 0 }
#define M(key, pic) { key, 2, (pic) * sizeof(char*), K_STRNULL, 0 }
// gCharacterStart, array 3 (CharacterValues.h)
#define A(key, member, i) { key, 3, offsetof(CharacterStart, member) + (i) * sizeof(int), K_INT, 0 }
#define L(key, member) { key, 3, offsetof(CharacterStart, member), K_STRING, 0 }
const Field kFields[] =
{
	F("Name",				szName,					K_STRING),
	F("Model",				szModelName,			K_STRING),
	F("Height",				fHeightStanding,		K_FLOAT),
	F("EyeLevel",			fEyeLevelStanding,		K_FLOAT),
	F("EyeLevelCrouching",	fEyeLevelCrouching,		K_FLOAT),
	F("EyeLevelDead",		fEyeLevelDead,			K_FLOAT),
	S("Skin",				MULTIPLAY_SKIN_NORMAL),
	S("SkinRed",			MULTIPLAY_SKIN_RED),
	S("SkinBlue",			MULTIPLAY_SKIN_BLUE),
	S("SkinGreen",			MULTIPLAY_SKIN_GREEN),
	S("SkinYellow",			MULTIPLAY_SKIN_YELLOW),
	S("SkinTeamBlue",		5),		// CalTEAM_B.dtx; SharedDefs.h names neither team slot
	S("SkinTeamRed",		6),		// CalTEAM_R.dtx
	M("Picture",			MULTIPLAY_SKIN_NORMAL),	// the character menu's; "" is none
	M("PictureRed",			MULTIPLAY_SKIN_RED),
	M("PictureBlue",		MULTIPLAY_SKIN_BLUE),
	M("PictureGreen",		MULTIPLAY_SKIN_GREEN),
	M("PictureYellow",		MULTIPLAY_SKIN_YELLOW),
	A("Strength",			nAttrib, 0),	// the menu's, and a new game's: 1-5
	A("Speed",				nAttrib, 1),
	A("Resistance",			nAttrib, 2),
	A("Magic",				nAttrib, 3),
	A("SwitchStrength",		nSwitch, 0),	// an add-on level's switch's: 0-5, 0 the above
	A("SwitchSpeed",		nSwitch, 1),
	A("SwitchResistance",	nSwitch, 2),
	A("SwitchMagic",		nSwitch, 3),
	L("StartWeapons",		szWeapons),		// WEAP_ names without the prefix
	L("StartItems",			szItems),		// INV_ names
	L("StartAmmo",			szAmmo),		// AMMO_ names, each with its count
	L("InitWeapons",		szInitWeapons),	// WEAP_ names, slots 1-9
};
#undef F
#undef S
#undef M
#undef A
#undef L
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const Array kArrays[] =
{
	{ gCharacterValues, sizeof(gCharacterValues[0]) },
	{ gCharacterSkins, sizeof(gCharacterSkins[0]) },
	{ gCharacterPictures, sizeof(gCharacterPictures[0]) },
	{ gCharacterStart, sizeof(gCharacterStart[0]) },
};

Table g_Table("Mod\\Characters.txt", "a Blood 2 character", "the CHARACTER_ names without the prefix",
			  "Blood2/ModReference/Characters.txt", kArrays, 4, MAX_CHARACTERS, CHARACTER_CALEB,
			  kCharacterNames, kNumNames, kFields, kNumFields,
			  BLOOD2_CHARACTER_IDS, "Base", "character");

} // namespace

const char* BloodModCharacter_Name(int nId)						{ return g_Table.Name(nId); }
int BloodModCharacter_Count()									{ return g_Table.Total(); }
int BloodModCharacter_Base(int nId)								{ return g_Table.Base(nId); }
int BloodModCharacter_Retail(int nId)							{ return g_Table.Retail(nId); }
int BloodModCharacter_Find(const char* pName)					{ return g_Table.Find(pName); }
const char* BloodModCharacter_Key(int nField)					{ return g_Table.Key(nField); }
std::string BloodModCharacter_Value(int nId, int nField, bool bRetail)	{ return g_Table.Value(nId, nField, bRetail); }
void BloodModCharacters_Clear()									{ g_Table.Clear(); }
std::string BloodModCharacters_Summary()						{ return g_Table.Summary(); }
std::string BloodModCharacters_Reference()						{ return g_Table.Reference(); }
std::string BloodModCharacters_Current()						{ return g_Table.Current(); }


// ---------------------------------------------------------------------------
// The start lists (CharacterValues.h)

namespace
{

const Named kItems[] =
{
	{ "MEDKIT", INV_MEDKIT }, { "FLASHLIGHT", INV_FLASHLIGHT }, { "NIGHTGOGGLES", INV_NIGHTGOGGLES },
	{ "THEEYE", INV_THEEYE }, { "BINOCULARS", INV_BINOCULARS },
	{ "PROXIMITY", INV_PROXIMITY }, { "REMOTE", INV_REMOTE }, { "TIMEBOMB", INV_TIMEBOMB },
	{ 0, 0 }
};

const Named kAmmo[] =
{
	{ "BULLET", AMMO_BULLET }, { "SHELL", AMMO_SHELL }, { "BMG", AMMO_BMG }, { "FLARE", AMMO_FLARE },
	{ "DIEBUGDIE", AMMO_DIEBUGDIE }, { "HOWITZER", AMMO_HOWITZER }, { "FUEL", AMMO_FUEL },
	{ "BATTERY", AMMO_BATTERY }, { "FOCUS", AMMO_FOCUS }, { "PROXIMITYBOMB", AMMO_PROXIMITYBOMB },
	{ "REMOTEBOMB", AMMO_REMOTEBOMB }, { "TIMEBOMB", AMMO_TIMEBOMB },
	{ 0, 0 }
};

std::vector<std::string> Words(const char* p)
{
	std::vector<std::string> out;
	std::string w;
	for (; p && *p; ++p)
	{
		if (*p == ' ' || *p == '\t' || *p == ',')
		{
			if (!w.empty()) out.push_back(w);
			w.clear();
		}
		else
			w += *p;
	}
	if (!w.empty()) out.push_back(w);
	return out;
}

int Lookup(const Named* pNames, const std::string& s)
{
	for (const Named* p = pNames; p->pName; ++p)
		if (LT1Mod::Registry::EqualNoCase(p->pName, s.c_str())) return p->nValue;
	return -1;
}

std::string Names(const Named* pNames)
{
	std::string s;
	for (const Named* p = pNames; p->pName; ++p)
		s += std::string(p == pNames ? "" : " ") + p->pName;
	return s;
}

// Every character's start values are good, or sError says which is not.
bool CheckStart(std::string& sError)
{
	static const char* const kAttrib[4] = { "Strength", "Speed", "Resistance", "Magic" };
	char buf[512];
	for (int nId = CHARACTER_CALEB; nId < BloodModCharacter_Count(); ++nId)
	{
		const CharacterStart& c = gCharacterStart[nId];
		const char* pName = BloodModCharacter_Name(nId);
		for (int i = 0; i < 4; ++i)
		{
			const char* pBad = c.nAttrib[i] < 1 || c.nAttrib[i] > 5 ? kAttrib[i] : 0;
			std::string sSwitch = std::string("Switch") + kAttrib[i];
			if (!pBad && (c.nSwitch[i] < 0 || c.nSwitch[i] > 5)) pBad = sSwitch.c_str();
			if (pBad)
			{
				snprintf(buf, sizeof(buf), "Mod\\Characters.txt: [%s]:%s is out of range (%s)", pName, pBad,
						 pBad == kAttrib[i] ? "1-5, as ValidateAttributes clamps" : "0-5; 0 is the character's own");
				sError = buf;
				return false;
			}
		}
		std::vector<int> w, it;
		std::vector<BloodModAmmo> am;
		std::string why;
		const char* pKey = 0;
		if (!BloodModCharacter_StartWeapons(nId, w, &why)) pKey = "StartWeapons";
		else if (!BloodModCharacter_StartItems(nId, it, &why)) pKey = "StartItems";
		else if (!BloodModCharacter_StartAmmo(nId, am, &why)) pKey = "StartAmmo";
		else if (!BloodModCharacter_InitWeapons(nId, w, &why)) pKey = "InitWeapons";
		if (pKey)
		{
			snprintf(buf, sizeof(buf), "Mod\\Characters.txt: [%s]:%s: %s", pName, pKey, why.c_str());
			sError = buf;
			return false;
		}
	}
	return true;
}

} // namespace

bool BloodModCharacter_StartWeapons(int nId, std::vector<int>& out, std::string* pWhy)
{
	out.clear();
	if (nId < CHARACTER_CALEB || nId >= BloodModCharacter_Count()) return false;
	const std::vector<std::string> words = Words(gCharacterStart[nId].szWeapons);
	for (size_t i = 0; i < words.size(); ++i)
	{
		const int nWeapon = BloodModWeapon_Find(words[i].c_str());
		if (nWeapon <= WEAP_NONE || BloodModWeapon_Retail(nWeapon) >= WEAP_BASEINVENTORY)
		{
			if (pWhy)
				*pWhy = "\"" + words[i] + "\" is " + (nWeapon > WEAP_NONE ? "a bomb, which StartItems gives" :
						"no weapon (the WEAP_ names without the prefix, Blood2/ModReference/Weapons.txt)");
			out.clear();
			return false;
		}
		out.push_back(nWeapon);
	}
	return true;
}

bool BloodModCharacter_InitWeapons(int nId, std::vector<int>& out, std::string* pWhy)
{
	out.clear();
	if (nId < CHARACTER_CALEB || nId >= BloodModCharacter_Count()) return false;
	const std::vector<std::string> words = Words(gCharacterStart[nId].szInitWeapons);
	for (size_t i = 0; i < words.size(); ++i)
	{
		const int nWeapon = BloodModWeapon_Find(words[i].c_str());
		const int nRetail = nWeapon > WEAP_NONE ? BloodModWeapon_Retail(nWeapon) : WEAP_NONE;
		if (nWeapon <= WEAP_NONE || nRetail >= WEAP_BASEINVENTORY || nRetail == WEAP_MELEE || i >= 9)
		{
			if (pWhy)
				*pWhy = i >= 9 ? std::string("more than nine weapons; there are slots 1-9") :
						"\"" + words[i] + "\" is " + (nWeapon <= WEAP_NONE ? "no weapon (the WEAP_ names without the prefix, "
						"Blood2/ModReference/Weapons.txt)" : nRetail == WEAP_MELEE ? "the melee weapon, which slot 0 always holds" :
						"a bomb, which a slot cannot hold");
			out.clear();
			return false;
		}
		out.push_back(nWeapon);
	}
	return true;
}

bool BloodMod_ParseItems(const char* pList, std::vector<int>& out, std::string* pWhy)
{
	out.clear();
	const std::vector<std::string> words = Words(pList);
	for (size_t i = 0; i < words.size(); ++i)
	{
		const int n = Lookup(kItems, words[i]);
		if (n < 0)
		{
			if (pWhy) *pWhy = "\"" + words[i] + "\" is no item (" + Names(kItems) + ")";
			out.clear();
			return false;
		}
		out.push_back(n);
	}
	return true;
}

bool BloodModCharacter_StartItems(int nId, std::vector<int>& out, std::string* pWhy)
{
	out.clear();
	if (nId < CHARACTER_CALEB || nId >= BloodModCharacter_Count()) return false;
	return BloodMod_ParseItems(gCharacterStart[nId].szItems, out, pWhy);
}

bool BloodMod_ParseAmmo(const char* pList, std::vector<BloodModAmmo>& out, std::string* pWhy)
{
	out.clear();
	const std::vector<std::string> words = Words(pList);
	for (size_t i = 0; i < words.size(); i += 2)
	{
		const int n = Lookup(kAmmo, words[i]);
		char* pEnd = 0;
		const double x = i + 1 < words.size() ? strtod(words[i + 1].c_str(), &pEnd) : -1.0;
		if (n < 0 || !pEnd || *pEnd || !(x >= 0.0 && x <= 1e6))
		{
			if (pWhy)
				*pWhy = n < 0 ? "\"" + words[i] + "\" is no ammo (" + Names(kAmmo) + ")"
							  : "\"" + words[i] + "\" wants a count, 0 or more, after it";
			out.clear();
			return false;
		}
		BloodModAmmo a = { n, (float)x };
		out.push_back(a);
	}
	return true;
}

bool BloodModCharacter_StartAmmo(int nId, std::vector<BloodModAmmo>& out, std::string* pWhy)
{
	out.clear();
	if (nId < CHARACTER_CALEB || nId >= BloodModCharacter_Count()) return false;
	return BloodMod_ParseAmmo(gCharacterStart[nId].szAmmo, out, pWhy);
}

void BloodModCharacter_Attributes(int nId, bool bSwitch, int nOut[4])
{
	if (nId < CHARACTER_CALEB || nId >= BloodModCharacter_Count())
	{
		nOut[0] = nOut[1] = nOut[2] = nOut[3] = 1;	// retail's default, before its switch
		return;
	}
	const CharacterStart& c = gCharacterStart[nId];
	for (int i = 0; i < 4; ++i)
		nOut[i] = bSwitch && c.nSwitch[i] ? c.nSwitch[i] : c.nAttrib[i];
}

// The table's own checks, then the start values': a bad name or attribute loads nothing, as a
// bad value of any other kind does.
bool BloodModCharacters_Load(LT1Mod::FileSource &src, std::string &sError)
{
	if (!g_Table.Load(src, sError))
		return false;
	if (!CheckStart(sError))
	{
		g_Table.Clear();
		return false;
	}
	return true;
}

void BloodModCharacters_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModCharacters_Load(src, sErr);
	if (!pPrint) return;
	PrintWrapped(bOk ? g_Table.Summary() : sErr,
				 std::string(pPrefix ? pPrefix : "") + (bOk ? "Mod\\Characters.txt: " : ""), pPrint);
}
