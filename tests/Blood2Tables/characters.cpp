// characters.cpp: testBlood2Tables for the characters' table, Mod\Characters.txt
// over gCharacterValues, gCharacterSkins and the menu's gCharacterPictures.
// The checks, for a table in three arrays:
//
//   names         every id below MAX_CHARACTERS has one section name, and the
//                 name finds it.
//   round trip    the --dump file, loaded as a mod, leaves the arrays as
//                 retail ships them, member by member through an independent
//                 list (kMembers, below), strings by content. A dump with one
//                 value moved, in either array, is caught here.
//   mutation      every key of every character moves its one member to its
//                 value; every member but the three nothing reads is a key's.
//   errors        a bad file loads nothing, over a mod loaded before it; the
//                 three unread fields are refused by name.
//   strings       a mod's strings outlive the next load (a player made under
//                 the first mod keeps the model name's pointer).
//   example mod   tests/Blood2Tables/characters_mod loads, and says what it
//                 changed.
//   menu          the pictures are retail's menu, entry for entry.
//   new           a section naming no character adds one with a Base: its
//                 values are the Base's but for its own keys, it is numbered
//                 after retail's, a chain ends at a retail character, and the
//                 arrays are full at BLOOD2_CHARACTER_IDS.

#include "server_de.h"
#include "SharedDefs.h"
#include "CharacterValues.h"
#include "BloodModCharacters.h"
#include "harness.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>

namespace
{

// Retail's three arrays, before anything writes them, including the empty
// elements past retail's.
CharacterValues	g_RetailValues[BLOOD2_CHARACTER_IDS];
char*			g_RetailSkins[BLOOD2_CHARACTER_IDS][MULTIPLAY_SKIN_MAX + 1];
char*			g_RetailPictures[BLOOD2_CHARACTER_IDS][BLOOD2_CHARACTER_PICTURES];

bool LoadCharacters(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\Characters.txt", text, err, BloodModCharacters_Load);
}

void Unload()
{
	BloodModCharacters_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// ---------------------------------------------------------------------------
// The members, transcribed from CharacterValues.h independently of the
// loader's key table: CharacterValues' nine, the seven skin slots, then the
// menu's five pictures.

enum MKind { M_STR, M_POD };
struct Member { const char* pName; int nArray; size_t nOffset, nSize; MKind eKind; };
#define S(m) { #m, 0, offsetof(CharacterValues, m), sizeof(char*), M_STR }
#define P(m) { #m, 0, offsetof(CharacterValues, m), sizeof(DFLOAT), M_POD }
#define K(n) { "skin" #n, 1, (n) * sizeof(char*), sizeof(char*), M_STR }
#define I(n) { "picture" #n, 2, (n) * sizeof(char*), sizeof(char*), M_STR }
const Member kMembers[] =
{
	S(szName), S(szModelName), S(szSkinName),
	P(fHeightStanding), P(fHeightCrouching), P(fHeightDead),
	P(fEyeLevelStanding), P(fEyeLevelCrouching), P(fEyeLevelDead),
	K(0), K(1), K(2), K(3), K(4), K(5), K(6),
	I(0), I(1), I(2), I(3), I(4),
};
#undef S
#undef P
#undef K
#undef I
const int kNumMembers = (int)(sizeof(kMembers) / sizeof(kMembers[0]));

const char* At(int nId, const Member& m, bool bRetail)
{
	if (m.nArray == 0)
		return (const char*)&(bRetail ? g_RetailValues : gCharacterValues)[nId] + m.nOffset;
	if (m.nArray == 1)
		return (const char*)(bRetail ? g_RetailSkins : gCharacterSkins)[nId] + m.nOffset;
	return (const char*)(bRetail ? g_RetailPictures : gCharacterPictures)[nId] + m.nOffset;
}

bool SameMember(int nId, const Member& m)
{
	const char* pa = At(nId, m, false);
	const char* pb = At(nId, m, true);
	if (m.eKind == M_POD) return memcmp(pa, pb, m.nSize) == 0;
	const char* sa = *(char* const*)pa;
	const char* sb = *(char* const*)pb;
	if (!sa || !sb) return sa == sb;
	return strcmp(sa, sb) == 0;
}

// Every character's differing members, "CALEB.fEyeLevelStanding".
std::vector<std::string> DifferAll()
{
	std::vector<std::string> out;
	for (int nId = 0; nId < MAX_CHARACTERS; ++nId)
		for (int m = 0; m < kNumMembers; ++m)
			if (!SameMember(nId, kMembers[m]))
				out.push_back(std::string(BloodModCharacter_Name(nId)) + "." + kMembers[m].pName);
	return out;
}

int NumKeys()
{
	int n = 0;
	while (BloodModCharacter_Key(n)) ++n;
	return n;
}

// ---------------------------------------------------------------------------

void TestNames()
{
	printf("characters: names\n");
	bool bAll = true;
	std::set<std::string> seen;
	for (int nId = 0; nId < MAX_CHARACTERS; ++nId)
	{
		const char* p = BloodModCharacter_Name(nId);
		if (!p || BloodModCharacter_Find(p) != nId || !seen.insert(p).second)
		{
			bAll = false;
			printf("        id %d: %s\n", nId, p ? p : "(no name)");
		}
	}
	char b[128];
	snprintf(b, sizeof(b), "every id 0-%d has one name, and the name finds it", MAX_CHARACTERS - 1);
	Check(bAll, b);
	Check(!BloodModCharacter_Name(-1) && !BloodModCharacter_Name(MAX_CHARACTERS),
		  "no name below CHARACTER_CALEB or at MAX_CHARACTERS");
	Check(BloodModCharacter_Find("caleb") == CHARACTER_CALEB && BloodModCharacter_Find("Gabreilla") == CHARACTER_GABREILLA &&
		  BloodModCharacter_Find("GABRIELLA") == -1 && BloodModCharacter_Find("Cal") == -1,
		  "names are matched without case, whole, and in the define's spelling (GABREILLA)");
}

void TestRoundTrip()
{
	printf("characters: round trip\n");
	std::string err;
	Unload();
	Check(DifferAll().empty(), "with no mod the arrays are retail");

	const std::string dump = CharactersDump();
	Check(LoadCharacters(dump, err), "the dump loads as a mod" + (err.empty() ? std::string() : ": " + err));
	const std::vector<std::string> d = DifferAll();
	char b[200];
	snprintf(b, sizeof(b), "every member of every character, %d x %d, is retail's with the dump loaded (moved: %s)",
			 kNumMembers, MAX_CHARACTERS, Join(d).c_str());
	Check(d.empty(), b);
	Check(BloodModCharacters_Summary().empty(), "and the summary says nothing changed");

	// The instrument, calibrated, once in each array.
	struct Bend { const char* label; const char* section; const char* key; const char* to; const char* expect; };
	const Bend bends[] =
	{
		{ "CALEB's EyeLevel",       "[CALEB]\n",     "\nEyeLevel = ",    "61.5",              "CALEB.fEyeLevelStanding" },
		{ "GABREILLA's SkinTeamRed", "[GABREILLA]\n", "\nSkinTeamRed = ", "\"Skins\\bent.dtx\"", "GABREILLA.skin6" },
	};
	for (size_t i = 0; i < sizeof(bends) / sizeof(bends[0]); ++i)
	{
		std::string bent = dump;
		const size_t at = bent.find(bends[i].section);
		const size_t k = at == std::string::npos ? at : bent.find(bends[i].key, at);
		const size_t e = k == std::string::npos ? k : bent.find('\n', k + 1);
		if (e != std::string::npos)
			bent.replace(k, e - k, std::string(bends[i].key) + bends[i].to);
		const bool bLoaded = LoadCharacters(bent, err);
		const std::vector<std::string> got = DifferAll();
		Check(bLoaded && got.size() == 1 && got[0] == bends[i].expect,
			  std::string("calibration: a dump with ") + bends[i].label + " moved is caught, there and only there (" +
			  Join(got) + ")");
	}
	Unload();
}

void TestMutation()
{
	printf("characters: mutation\n");
	std::string err;
	int nCases = 0, nBad = 0;
	std::set<std::string> reached;
	const int nKeys = NumKeys();
	for (int nId = 0; nId < MAX_CHARACTERS; ++nId)
		for (int f = 0; f < nKeys; ++f)
		{
			const char* key = BloodModCharacter_Key(f);
			const std::string sRetail = BloodModCharacter_Value(nId, f, true);
			const std::string sNew = Other(key, sRetail);
			const std::string text = std::string("[") + BloodModCharacter_Name(nId) + "]\n" + key + " = " + sNew + "\n";
			++nCases;
			const bool bLoaded = LoadCharacters(text, err);
			const std::vector<std::string> d = DifferAll();
			const std::string sum = BloodModCharacters_Summary();
			const std::string want = std::string(BloodModCharacter_Name(nId)) + ": " + key + " ";
			const std::string got = BloodModCharacter_Value(nId, f);
			const double dNew = atof(sNew.c_str()), dGot = atof(got.c_str());
			const bool bValue = sNew[0] == '"' ? got == sNew : fabs(dGot - dNew) <= 1e-6 * (1.0 + fabs(dNew));
			const bool bOk = bLoaded && d.size() == 1 && sum.compare(0, want.size(), want) == 0 &&
							 sum.find('\n') == std::string::npos && bValue;
			if (bOk)
				reached.insert(d[0].substr(d[0].find('.') + 1));
			else if (++nBad <= 8)
				printf("        [%s] %s = %s: loaded %d, moved %s, value %s, summary \"%s\"%s\n",
					   BloodModCharacter_Name(nId), key, sNew.c_str(), (int)bLoaded, Join(d).c_str(), got.c_str(),
					   sum.c_str(), err.empty() ? "" : (" err " + err).c_str());
		}
	char b[200];
	snprintf(b, sizeof(b), "%d single-field overrides (%d keys x %d characters): each moves its one member to its "
			 "value, and the summary names it", nCases, nKeys, MAX_CHARACTERS);
	Check(nBad == 0, b);
	std::vector<std::string> unreached;
	for (int m = 0; m < kNumMembers; ++m)
		if (!reached.count(kMembers[m].pName)) unreached.push_back(kMembers[m].pName);
	Check(Join(unreached) == "szSkinName, fHeightCrouching, fHeightDead",
		  "every member is some key's but the three nothing reads (none: " + Join(unreached) + ")");
	Unload();
}

void TestErrors()
{
	printf("characters: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a section naming no character",	"[CABAL]\nEyeLevel = 1.0\n",				"is not a Blood 2 character" },
		{ "Gabriella spelt right",			"[GABRIELLA]\nEyeLevel = 1.0\n",			"the CHARACTER_ names without the prefix" },
		{ "szSkinName, which nothing reads",	"[CALEB]\nSkinName = \"x.dtx\"\n",		"has no field \"SkinName\"" },
		{ "fHeightCrouching, likewise",		"[CALEB]\nHeightCrouching = 20.0\n",		"has no field \"HeightCrouching\"" },
		{ "a number for a string",			"[CALEB]\nModel = 3\n",					"is not a quoted string" },
		{ "a string for a number",			"[CALEB]\nHeight = \"82\"\n",				"is not a number" },
		{ "a negative eye level",			"[CALEB]\nEyeLevel = -1.0\n",				"EyeLevel is out of range" },
		{ "a 100-character skin",			0,											"longer than 99 characters" },
	};
	const std::string sLong = "[OPHELIA]\nSkinBlue = \"" + std::string(100, 'x') + "\"\n";
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadCharacters("[ISHMAEL]\nEyeLevel = 40.0\n", err);
		const bool bBefore = DifferAll().size() == 1;
		err.clear();
		const bool bLoaded = LoadCharacters(bad[i].text ? bad[i].text : sLong, err);
		Check(bBefore && !bLoaded && DifferAll().empty() && err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	std::string err;
	Check(MountAndLoad(0, "", err, BloodModCharacters_Load) && err.empty() && DifferAll().empty(),
		  "a mod without Characters.txt sets the arrays back to retail, and is no error");
	Unload();
}

void TestStrings()
{
	printf("characters: strings\n");
	std::string err;
	LoadCharacters("[CALEB]\nModel = \"Models\\Characters\\Ophelia.abc\"\nSkinRed = \"Skins\\Mod\\red.dtx\"\n", err);
	const char* pModel = gCharacterValues[CHARACTER_CALEB].szModelName;
	const char* pSkin = gCharacterSkins[CHARACTER_CALEB][MULTIPLAY_SKIN_RED];
	Check(pModel && !strcmp(pModel, "Models\\Characters\\Ophelia.abc") && pSkin && !strcmp(pSkin, "Skins\\Mod\\red.dtx"),
		  "a mod's strings are in both arrays, a backslash as one backslash");
	LoadCharacters("[CALEB]\nModel = \"Models\\Characters\\second.abc\"\n", err);
	Check(!strcmp(pModel, "Models\\Characters\\Ophelia.abc") && !strcmp(pSkin, "Skins\\Mod\\red.dtx"),
		  "and outlive a later load");
	Check(gCharacterSkins[CHARACTER_CALEB][MULTIPLAY_SKIN_RED] == g_RetailSkins[CHARACTER_CALEB][MULTIPLAY_SKIN_RED],
		  "a load starts from retail: the first mod's skin is gone from the table");
	Check(BloodModCharacters_Summary() == "CALEB: Model \"Models\\Characters\\second.abc\"",
		  "and the summary says what the second mod changed: " + BloodModCharacters_Summary());
	Unload();
}

// The example mod, tests/Blood2Tables/characters_mod, as the server loads it.
void TestCharactersMod()
{
	printf("characters: the example mod\n");
	std::string err;
	const std::string text = Slurp(BLOOD2_CHARACTERS_MOD);
	Check(!text.empty(), std::string("read ") + BLOOD2_CHARACTERS_MOD);
	const bool bLoaded = LoadCharacters(text, err);
	const std::string sum = BloodModCharacters_Summary();
	Check(bLoaded && sum == "CALEB: EyeLevel 70.0, EyeLevelCrouching 30.0",
		  "it loads, and the summary the console prints is: " + sum + (err.empty() ? std::string() : " " + err));
	Check(Join(DifferAll()) == "CALEB.fEyeLevelStanding, CALEB.fEyeLevelCrouching" &&
		  g_RetailValues[CHARACTER_CALEB].fEyeLevelStanding == 36.0f,
		  "two members moved, no others, and retail's standing eye was 36: " + Join(DifferAll()));
	Unload();
}

// The menu's pictures are retail's g_CharacterMenuInfo, transcribed here from
// ClientShellDLL/MenuCharacter.cpp as it was before the table took it over,
// in the order the menu builds them: character by character, color by color.
void TestMenu()
{
	printf("characters: the menu's pictures\n");
	struct Pic { const char* pFile; int nChar, nColor; };
#define D "interface\\charscreen\\characterpics\\"
	const Pic kRetailMenu[] =
	{
		{ D "caleb_single.pcx", 0, 0 }, { D "caleb_red.pcx", 0, 1 }, { D "caleb_blue.pcx", 0, 2 }, { D "caleb_green.pcx", 0, 3 }, { D "caleb_yellow.pcx", 0, 4 },
		{ D "ophelia_single.pcx", 1, 0 }, { D "ophelia_red.pcx", 1, 1 }, { D "ophelia_blue.pcx", 1, 2 }, { D "ophelia_green.pcx", 1, 3 }, { D "ophelia_yellow.pcx", 1, 4 },
		{ D "ishmael_single.pcx", 2, 0 }, { D "ishmael_red.pcx", 2, 1 }, { D "ishmael_blue.pcx", 2, 2 }, { D "ishmael_green.pcx", 2, 3 }, { D "ishmael_yellow.pcx", 2, 4 },
		{ D "gabby_single.pcx", 3, 0 }, { D "gabby_red.pcx", 3, 1 }, { D "gabby_blue.pcx", 3, 2 }, { D "gabby_green.pcx", 3, 3 }, { D "gabby_yellow.pcx", 3, 4 },
#ifdef _ADD_ON
		{ "interface_AO\\m_cultist1.pcx", 4, 0 }, { "interface_AO\\clown2.pcx", 4, 1 }, { "interface_AO\\clown1.pcx", 4, 2 },
		{ "interface_AO\\clown3.pcx", 4, 3 }, { "interface_AO\\m_cultist3.pcx", 4, 4 },
		{ "interface_AO\\f_cultist1.pcx", 5, 0 }, { "interface_AO\\souldrudge1.pcx", 6, 0 }, { "interface_AO\\prophet.pcx", 7, 0 },
#endif
	};
#undef D
	const int nRetail = (int)(sizeof(kRetailMenu) / sizeof(kRetailMenu[0]));
	// BuildCharacterMenuInfo's walk, over the table.
	std::vector<Pic> got;
	for (int c = 0; c < BloodModCharacter_Count(); ++c)
		for (int p = 0; p < BLOOD2_CHARACTER_PICTURES; ++p)
			if (gCharacterPictures[c][p]) { Pic x = { gCharacterPictures[c][p], c, p }; got.push_back(x); }
	bool bSame = (int)got.size() == nRetail;
	for (int i = 0; bSame && i < nRetail; ++i)
		bSame = !strcmp(got[(size_t)i].pFile, kRetailMenu[i].pFile) && got[(size_t)i].nChar == kRetailMenu[i].nChar &&
				got[(size_t)i].nColor == kRetailMenu[i].nColor;
	char b[160];
	snprintf(b, sizeof(b), "the table's pictures are retail's menu, all %d entries in order (%d built)", nRetail, (int)got.size());
	Check(bSame, b);
}

// A section naming no character, with a Base, adds one after retail's.
void TestNewCharacters()
{
	printf("characters: new characters\n");
	std::string err;
	const int kFirst = MAX_CHARACTERS;
	Unload();
	Check(BloodModCharacter_Count() == MAX_CHARACTERS && BloodModCharacter_Base(CHARACTER_CALEB) == -1 &&
		  BloodModCharacter_Retail(CHARACTER_OPHELIA) == CHARACTER_OPHELIA,
		  "with no mod there are retail's characters, none with a Base, each its own retail character");

	// Every retail character as a Base, with nothing else: the new one is its
	// Base on every member.
	int nBad = 0;
	for (int nBase = 0; nBase < MAX_CHARACTERS; ++nBase)
	{
		const std::string text = std::string("[NEWONE]\nBase = \"") + BloodModCharacter_Name(nBase) + "\"\n";
		const bool bLoaded = LoadCharacters(text, err);
		bool bSame = bLoaded && BloodModCharacter_Count() == kFirst + 1 && BloodModCharacter_Find("newone") == kFirst &&
					 BloodModCharacter_Base(kFirst) == nBase && BloodModCharacter_Retail(kFirst) == nBase &&
					 DifferAll().empty();
		for (int m = 0; bSame && m < kNumMembers; ++m)
		{
			const char* pa = At(kFirst, kMembers[m], false);
			const char* pb = At(nBase, kMembers[m], true);
			if (kMembers[m].eKind == M_POD) bSame = memcmp(pa, pb, kMembers[m].nSize) == 0;
			else
			{
				const char* sa = *(char* const*)pa;
				const char* sb = *(char* const*)pb;
				bSame = (!sa || !sb) ? sa == sb : !strcmp(sa, sb);
			}
		}
		if (!bSame && ++nBad <= 4)
			printf("        Base %s: loaded %d, count %d, %s\n", BloodModCharacter_Name(nBase), (int)bLoaded,
				   BloodModCharacter_Count(), err.c_str());
	}
	char b[200];
	snprintf(b, sizeof(b), "a new character on each of the %d retail ones is id %d, its Base on all %d members, and "
			 "changes no retail one", MAX_CHARACTERS, kFirst, kNumMembers);
	Check(nBad == 0, b);

	// Its own keys, over its Base as this mod leaves it; and a chain.
	const bool bLoaded = LoadCharacters(
		"[CALEB]\nEyeLevel = 50.0\n"
		"[SCARLET]\nBase = \"CALEB\"\nSkinRed = \"Skins\\Mod\\scarlet_red.dtx\"\nPicture = \"interface\\mod\\scarlet.pcx\"\n"
		"[CRIMSON]\nBase = \"scarlet\"\nEyeLevelCrouching = 14.0\nPictureBlue = \"\"\n", err);
	Check(bLoaded && BloodModCharacter_Count() == kFirst + 2 && BloodModCharacter_Find("CRIMSON") == kFirst + 1 &&
		  BloodModCharacter_Base(kFirst + 1) == kFirst && BloodModCharacter_Retail(kFirst + 1) == CHARACTER_CALEB,
		  "two new ones are numbered in file order, and a chain of Bases ends at a retail character" +
		  (err.empty() ? std::string() : ": " + err));
	Check(gCharacterValues[kFirst].fEyeLevelStanding == 50.0f && gCharacterValues[kFirst + 1].fEyeLevelStanding == 50.0f &&
		  gCharacterValues[kFirst + 1].fEyeLevelCrouching == 14.0f &&
		  !strcmp(gCharacterSkins[kFirst + 1][MULTIPLAY_SKIN_RED], "Skins\\Mod\\scarlet_red.dtx") &&
		  !strcmp(gCharacterSkins[kFirst + 1][MULTIPLAY_SKIN_BLUE], g_RetailSkins[CHARACTER_CALEB][MULTIPLAY_SKIN_BLUE]) &&
		  !strcmp(gCharacterPictures[kFirst + 1][0], "interface\\mod\\scarlet.pcx") &&
		  !gCharacterPictures[kFirst + 1][MULTIPLAY_SKIN_BLUE] && gCharacterPictures[kFirst][MULTIPLAY_SKIN_BLUE],
		  "each takes its Base as the mod leaves it (CALEB's eye level), then its own keys; \"\" is no picture");
	const std::string sum = BloodModCharacters_Summary();
	Check(sum == "CALEB: EyeLevel 50.0\n"
				 "SCARLET: new character " + std::to_string(kFirst) + ", Base \"CALEB\", SkinRed \"Skins\\Mod\\scarlet_red.dtx\", Picture \"interface\\mod\\scarlet.pcx\"\n"
				 "CRIMSON: new character " + std::to_string(kFirst + 1) + ", Base \"SCARLET\", EyeLevelCrouching 14.0, PictureBlue \"\"",
		  "and the summary names each with its number and Base: " + sum);

	// The arrays' room, and the errors.
	std::string full;
	for (int n = 0; n < BLOOD2_CHARACTER_IDS - MAX_CHARACTERS; ++n)
		full += "[EXTRA" + std::to_string(n) + "]\nBase = \"ISHMAEL\"\n";
	Check(LoadCharacters(full, err) && BloodModCharacter_Count() == BLOOD2_CHARACTER_IDS,
		  "the arrays hold new ones up to BLOOD2_CHARACTER_IDS" + (err.empty() ? std::string() : ": " + err));
	struct Bad { const char* label; std::string text; const char* expect; };
	const Bad bad[] =
	{
		{ "one more than that",			full + "[EXTRAMORE]\nBase = \"ISHMAEL\"\n",	"one new entry too many" },
		{ "a Base nothing names",		"[SCARLET]\nBase = \"CABAL\"\n",				"is not a Blood 2 character, nor a new one above it" },
		{ "a Base below it in the file",	"[A]\nBase = \"B\"\n[B]\nBase = \"CALEB\"\n",	"is not a Blood 2 character, nor a new one above it" },
		{ "a name longer than 31",		"[" + std::string(32, 'N') + "]\nBase = \"CALEB\"\n",	"a new name is letters, digits and _" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		LoadCharacters("[ISHMAEL]\nEyeLevel = 40.0\n", err);
		err.clear();
		const bool bBad = LoadCharacters(bad[i].text, err);
		Check(!bBad && BloodModCharacter_Count() == MAX_CHARACTERS && DifferAll().empty() &&
			  err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing: " + err);
	}
	Unload();
	Check(BloodModCharacter_Count() == MAX_CHARACTERS && !BloodModCharacter_Name(kFirst),
		  "and with the mod gone the new ones are gone");
}

} // namespace

std::string CharactersDump()
{
	return
		"// Blood II's player characters, every value retail ships: the reference\n"
		"// for a mod's Mod\\Characters.txt. GENERATED by `testBlood2Tables --dump\n"
		"// characters` from Blood2/Shared/CharacterValues.cpp; do not edit,\n"
		"// regenerate. testBlood2Tables proves that loading this file as a mod\n"
		"// changes nothing.\n"
		"//\n"
		"// A mod names only what it changes:\n"
		"//\n"
		"//     [CALEB]\n"
		"//     EyeLevel = 40.0\n"
		"//\n"
		"// A section is the character's CHARACTER_ name (Shared/SharedDefs.h)\n"
		"// without the prefix, in the define's spelling: GABREILLA.  Name is the\n"
		"// sound directory, sounds\\chosen\\<Name>; Height sets only the first\n"
		"// dims; Skin .. SkinTeamRed are the multiplayer skins, by\n"
		"// MULTIPLAY_SKIN_, Skin the single-player one; Picture .. PictureYellow\n"
		"// the character menu's, \"\" for none.  Every number is 0 or more; a\n"
		"// string is at most 99 characters.\n"
		"//\n"
		"// A section naming no character adds one, on a Base (a character here\n"
		"// or a new one above it) as the mod leaves that one:\n"
		"//\n"
		"//     [SCARLET]\n"
		"//     Base = \"CALEB\"\n"
		"//     SkinRed = \"Skins\\Mod\\scarlet_red.dtx\"\n"
		"//\n"
		"// Blood2/Shared/BloodModCharacters.h has the rest.\n"
		+ BloodModCharacters_Reference();
}

void CharactersSelfTest()
{
	memcpy(g_RetailValues, gCharacterValues, sizeof(g_RetailValues));
	memcpy(g_RetailSkins, gCharacterSkins, sizeof(g_RetailSkins));
	memcpy(g_RetailPictures, gCharacterPictures, sizeof(g_RetailPictures));
	Check(kNumMembers == 21, "kMembers lists CharacterValues' 9 members, the 7 skin slots and the 5 pictures");
	Check(sizeof(CharacterValues) == 3 * sizeof(char*) + 6 * sizeof(DFLOAT) ||
		  sizeof(CharacterValues) == 4 * sizeof(char*) + 6 * sizeof(DFLOAT),
		  "and CharacterValues is those nine (at most padding after them)");
	TestNames();
	TestRoundTrip();
	TestMutation();
	TestErrors();
	TestStrings();
	TestCharactersMod();
	TestMenu();
	TestNewCharacters();
	Check(memcmp(g_RetailValues, gCharacterValues, sizeof(g_RetailValues)) == 0 &&
		  memcmp(g_RetailSkins, gCharacterSkins, sizeof(g_RetailSkins)) == 0 &&
		  memcmp(g_RetailPictures, gCharacterPictures, sizeof(g_RetailPictures)) == 0,
		  "and all three arrays are byte for byte retail's at the end, past retail's characters too");
}
