// testBlood2Tables: Blood II's mod-overridable tables against retail
//
//   testBlood2Tables --selftest
//   testBlood2Tables --load-mod <dir>                  a mod folder's tables, as the shells load them
//   testBlood2Tables --dump  weapons|characters|ai|strings <out.txt>   Blood2/ModReference/<Table>.txt
//   testBlood2Tables --check weapons|characters|ai|strings <file>      0 if <file> is what --dump writes
//
// Built from the game's own tables (Shared/WeaponDefs.cpp, Shared/CharacterValues.cpp) and the
// BloodMod*.cpp loaders, which the modules compile. characters.cpp tests Mod\Characters.txt, ai.cpp
// the server's Mod\AI.txt and strings.cpp the client's Mod\Strings.txt, against the built string
// module. --selftest, for the weapons:
//
//   names         every weapon id from 1 to WEAP_MAXWEAPONTYPES has one section name, and the
//                 name finds it.
//   round trip    the --dump file, loaded as a mod, leaves g_WeaponDefaults as retail ships it
//                 (every member, compared through an independent list of WeaponData's members,
//                 strings by content) and the summary empty. Calibrated: a dump with one value
//                 moved is caught, at that member.
//   mutation      for every weapon and every key, a mod setting that key alone to another value
//                 changes that one member to that value and nothing else, and the summary names
//                 that weapon and key. Every member but m_nType is some key's.
//   errors        a bad file loads nothing: the table is retail afterwards, even over a mod
//                 loaded before it.
//   strings       a mod's strings outlive the next load (CWeapon keeps the table's pointers);
//                 "" is NULL in the six fields retail leaves NULL.
//   example mod   tests/Blood2Tables/balance_mod loads, and says what it changed.
//   new weapons   a section naming no weapon adds one on a Base.

#include "server_de.h"
#include "WeaponDefs.h"
#include "BloodModWeapons.h"
#include "BloodModCharacters.h"
#include "BloodModAI.h"
#include "BloodModStrings.h"
#include "lt1mod_startcmds.h"
#include "lt1mod_script.h"
#include "harness.h"

#include "lt1mod.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

int g_nPass = 0, g_nFail = 0;

void Check(bool b, const std::string& what)
{
	if (b) { ++g_nPass; printf("  ok    %s\n", what.c_str()); }
	else   { ++g_nFail; printf("  FAIL  %s\n", what.c_str()); }
}

bool MemSource::ReadFile(const char* path, std::string& out)
{
	std::map<std::string, std::string>::iterator it = files.find(path);
	if (it == files.end()) return false;
	out = it->second;
	return true;
}

void MemSource::ListDir(const char* dir, std::vector<LT1Mod::DirEntry>& out)
{
	out.clear();
	std::string prefix = std::string(dir) + "\\";
	for (std::map<std::string, std::string>::iterator it = files.begin(); it != files.end(); ++it)
		if (it->first.compare(0, prefix.size(), prefix) == 0 &&
			it->first.find('\\', prefix.size()) == std::string::npos)
		{
			LT1Mod::DirEntry e; e.path = it->first; e.isDir = false; out.push_back(e);
		}
}

bool MountAndLoad(const char* pPath, const std::string& text, std::string& err,
				  bool (*pLoad)(LT1Mod::FileSource&, std::string&))
{
	MemSource src;
	src.files["Mod\\Mod.txt"] = "[Mod]\nId = \"Test\"\nGame = \"Blood2\"\n";
	if (pPath) src.files[pPath] = text;
	LT1Mod::State s;
	LT1Mod::Load(src, "Blood2", s);
	LT1Mod::SetActive(s);
	return pLoad(src, err);
}

std::string Join(const std::vector<std::string>& v)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + v[i];
	return s.empty() ? std::string("none") : s;
}

std::string Slurp(const char* pPath)
{
	std::string text;
	FILE* f = fopen(pPath, "rb");
	if (!f) return text;
	char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
	fclose(f);
	return text;
}

namespace
{

// Mounts a mod holding Weapons.txt = text and loads it
bool LoadWeapons(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\Weapons.txt", text, err, BloodModWeapons_Load);
}

void Unload()
{
	BloodModWeapons_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// WeaponData's members, transcribed from WeaponDefs.h independently of the loader's key table, so
// a key written to the wrong offset, or a member no key reaches, shows

enum MKind { M_STR, M_POD };
struct Member { const char* pName; size_t nOffset, nSize; MKind eKind; };
#define S(m) { #m, offsetof(WeaponData, m), sizeof(((WeaponData*)0)->m), M_STR }
#define P(m) { #m, offsetof(WeaponData, m), sizeof(((WeaponData*)0)->m), M_POD }
const Member kMembers[] =
{
	S(m_szViewModelFilename), S(m_szLeftViewModelFilename), S(m_szViewModelSkin),
	S(m_szHandModelFilename), S(m_szHandModelSkin),
	P(m_nType), P(m_nFireType), P(m_nAmmoType), P(m_nAmmoUse), P(m_nAltAmmoUse),
	P(m_fMinDamage), P(m_fMaxDamage), P(m_fMinAltDamage), P(m_fMaxAltDamage),
	P(m_fReloadTime), P(m_fAltReloadTime), P(m_Spread), P(m_AltSpread),
	P(m_fProjVelocity), P(m_fAltProjVelocity), P(m_fRange), P(m_fAltRange),
	P(m_dwShotsPerFire), P(m_dwAltShotsPerFire), P(m_dwStrengthReq), P(m_dwTwoHandStrengthReq),
	P(m_nDamageRadius), P(m_nAltDamageRadius), P(m_bAltFireZoom), P(m_bSemiAuto),
	S(m_szFireSound), S(m_szAltFireSound), S(m_szEmptyWeaponSound), S(m_szAltEmptyWeaponSound),
	S(m_szProjectileClass), S(m_szAltProjectileClass),
	P(m_fFlashRadius), P(m_vFlashColor),
	S(m_szWeaponName), P(m_nWeaponNameID), S(m_szFlashSprite), S(m_szAltFlashSprite),
	P(m_fFlashDuration), P(m_fFlashScale),
	P(m_vHandModelOffset), P(m_vViewModelOffset), P(m_vMuzzleOffset), P(m_vRecoil), P(m_vFlash),
	P(m_fEjectInterval), P(m_fViewKick), P(m_bCumulativeKick), P(m_bLoopAnim), P(m_bAltLoopAnim),
	S(m_szStatusIcon), S(m_szStatusIconH),
};
#undef S
#undef P
const int kNumMembers = (int)(sizeof(kMembers) / sizeof(kMembers[0]));

bool SameMember(const WeaponData& a, const WeaponData& b, const Member& m)
{
	const char* pa = (const char*)&a + m.nOffset;
	const char* pb = (const char*)&b + m.nOffset;
	if (m.eKind == M_POD) return memcmp(pa, pb, m.nSize) == 0;
	const char* sa = *(char* const*)pa;
	const char* sb = *(char* const*)pb;
	if (!sa || !sb) return sa == sb;
	return strcmp(sa, sb) == 0;
}

// The members of weapon nId that differ from retail's copy, by name
std::vector<std::string> Differ(const WeaponData* pRetail, int nId)
{
	std::vector<std::string> out;
	for (int m = 0; m < kNumMembers; ++m)
		if (!SameMember(g_WeaponDefaults[nId - 1], pRetail[nId - 1], kMembers[m]))
			out.push_back(kMembers[m].pName);
	return out;
}

// Every weapon's differing members, "BERETTA.m_fMinDamage", over the table
std::vector<std::string> DifferAll(const WeaponData* pRetail)
{
	std::vector<std::string> out;
	for (int nId = 1; nId <= WEAP_MAXWEAPONTYPES; ++nId)
	{
		const std::vector<std::string> d = Differ(pRetail, nId);
		for (size_t i = 0; i < d.size(); ++i)
			out.push_back(std::string(BloodModWeapon_Name(nId)) + "." + d[i]);
	}
	return out;
}

// The dump

std::string Dump()
{
	return
		"// Blood II's weapons, every value retail ships: the reference for a mod's\n"
		"// Mod\\Weapons.txt. GENERATED by `testBlood2Tables --dump weapons` from\n"
		"// Blood2/Shared/WeaponDefs.cpp; do not edit, regenerate. testBlood2Tables\n"
		"// proves that loading this file as a mod changes nothing.\n"
		"//\n"
		"// A mod names only what it changes:\n"
		"//\n"
		"//     [BERETTA]\n"
		"//     MinDamage = 16.0\n"
		"//     MaxDamage = 24.0\n"
		"//\n"
		"// A section is the weapon's WEAP_ name (Shared/SharedDefs.h) without\n"
		"// the prefix; the creature weapons are in the table too.  In\n"
		"// HandModel, HandSkin, ProjectileClass, AltProjectileClass, StatusIcon\n"
		"// and StatusIconH, \"\" means none (NULL: a ProjectileClass of \"\" is a\n"
		"// vector weapon).  FireType is RIFLE, AUTORIFLE, PISTOL, MELEE or MAGIC;\n"
		"// AmmoType one of AMMO_'s names.  Every number is 0 or more; a string\n"
		"// is at most 99 characters. A section naming no weapon adds one on a Base\n"
		"// (Blood2/Shared/BloodModWeapons.h).\n"
		+ BloodModWeapons_Reference();
}

} // namespace

// A value that differs from v, of the same kind, as Weapons.txt writes it; v is the reference's
// text for the key
std::string Other(const std::string& key, const std::string& v)
{
	char b[160];
	if (key == "FireType")	return v == "\"PISTOL\"" ? "\"MAGIC\"" : "\"PISTOL\"";
	if (key == "AmmoType")	return v == "\"FUEL\"" ? "\"TIMEBOMB\"" : "\"FUEL\"";
	if (v == "TRUE")		return "FALSE";
	if (v == "FALSE")		return "TRUE";
	if (v[0] == '"')		return v == "\"\"" ? "\"Mod\\Test\\value.abc\"" : "\"" + v.substr(1, v.size() - 2) + "_mod\"";
	if (v[0] == '<')
	{
		float x, y, z;
		sscanf(v.c_str(), "<%f, %f, %f>", &x, &y, &z);
		snprintf(b, sizeof(b), "<%.3f, %.3f, %.3f>", x + 1.25f, y - 0.5f, z + 2.0f);
		return b;
	}
	if (v.find('.') != std::string::npos)
	{
		snprintf(b, sizeof(b), "%.4f", atof(v.c_str()) * 2.0 + 1.5);
		return b;
	}
	snprintf(b, sizeof(b), "%ld", atol(v.c_str()) + 1);
	return b;
}

namespace
{

void TestNames()
{
	printf("names\n");
	bool bAll = true;
	std::set<std::string> seen;
	for (int nId = 1; nId <= WEAP_MAXWEAPONTYPES; ++nId)
	{
		const char* p = BloodModWeapon_Name(nId);
		if (!p || BloodModWeapon_Find(p) != nId || !seen.insert(p).second)
		{
			bAll = false;
			printf("        id %d: %s\n", nId, p ? p : "(no name)");
		}
	}
	char b[128];
	snprintf(b, sizeof(b), "every id 1-%d has one name, and the name finds it", (int)WEAP_MAXWEAPONTYPES);
	Check(bAll, b);
	Check(!BloodModWeapon_Name(0) && !BloodModWeapon_Name(WEAP_MAXWEAPONTYPES + 1),
		  "no name below 1 or past WEAP_MAXWEAPONTYPES");
	Check(BloodModWeapon_Find("beretta") == WEAP_BERETTA && BloodModWeapon_Find("SoUl_AxE") == WEAP_SOUL_AXE &&
		  BloodModWeapon_Find("Beretta92F") == 0, "names are matched without case, and only whole");
	Check(BloodModWeapon_Find("TIMEBOMB") == WEAP_TIMEBOMB && WEAP_TIMEBOMB == WEAP_MAXWEAPONTYPES,
		  "the last id, TIMEBOMB, is WEAP_MAXWEAPONTYPES");
}

void TestRoundTrip(const WeaponData* pRetail)
{
	printf("round trip\n");
	std::string err;
	Unload();
	Check(DifferAll(pRetail).empty(), "with no mod the table is retail");

	const std::string dump = Dump();
	Check(LoadWeapons(dump, err), "the dump loads as a mod" + (err.empty() ? std::string() : ": " + err));
	const std::vector<std::string> d = DifferAll(pRetail);
	char b[160];
	snprintf(b, sizeof(b), "every member of every weapon, %d x %d, is retail's with the dump loaded (moved: %s)",
			 kNumMembers, (int)WEAP_MAXWEAPONTYPES, Join(d).c_str());
	Check(d.empty(), b);
	Check(BloodModWeapons_Summary().empty(), "and the summary says nothing changed");
	bool bNamed = true;
	for (int nId = 1; nId <= WEAP_MAXWEAPONTYPES; ++nId)
		for (int f = 0; BloodModWeapon_Key(f); ++f)
			if ((!strcmp(BloodModWeapon_Key(f), "FireType") || !strcmp(BloodModWeapon_Key(f), "AmmoType")) &&
				BloodModWeapon_Value(nId, f, true)[0] != '"')
				bNamed = false;
	Check(bNamed, "every FireType and AmmoType retail uses has a name");

	// The instrument, calibrated: a dump with one value moved is caught there
	struct Bend { const char* label; const char* from; const char* to; const char* expect; };
	const Bend bends[] =
	{
		{ "BERETTA's MinDamage", "[BERETTA]\nViewModel", 0, "BERETTA.m_fMinDamage" },
		{ "SHOTGUN's AltSpreadV", "[SHOTGUN]\nViewModel", 0, "SHOTGUN.m_AltSpread" },
		{ "SKULL's StatusIcon", "[SKULL]\nViewModel", 0, "SKULL.m_szStatusIcon" },
	};
	for (size_t i = 0; i < sizeof(bends) / sizeof(bends[0]); ++i)
	{
		std::string bent = dump;
		const size_t at = bent.find(bends[i].from);
		const char* key = i == 0 ? "\nMinDamage = " : i == 1 ? "\nAltSpreadV = " : "\nStatusIcon = ";
		const size_t k = at == std::string::npos ? at : bent.find(key, at);
		const size_t e = k == std::string::npos ? k : bent.find('\n', k + 1);
		if (e != std::string::npos)
			bent.replace(k, e - k, std::string(key) + (i == 2 ? "\"interface\\bent.pcx\"" : "1234.5"));
		const bool bLoaded = LoadWeapons(bent, err);
		const std::vector<std::string> got = DifferAll(pRetail);
		Check(bLoaded && got.size() == 1 && got[0] == bends[i].expect,
			  std::string("calibration: a dump with ") + bends[i].label + " moved is caught, there and only there (" +
			  Join(got) + ")");
	}
	Unload();
}

void TestMutation(const WeaponData* pRetail)
{
	printf("mutation\n");
	std::string err;
	int nCases = 0, nBad = 0;
	std::set<std::string> reached;		// members some key changes
	int nKeys = 0;
	for (int f = 0; BloodModWeapon_Key(f); ++f) ++nKeys;
	for (int nId = 1; nId <= WEAP_MAXWEAPONTYPES; ++nId)
		for (int f = 0; f < nKeys; ++f)
		{
			const char* key = BloodModWeapon_Key(f);
			const std::string sRetail = BloodModWeapon_Value(nId, f, true);
			const std::string sNew = Other(key, sRetail);
			const std::string text = std::string("[") + BloodModWeapon_Name(nId) + "]\n" + key + " = " + sNew + "\n";
			++nCases;
			const bool bLoaded = LoadWeapons(text, err);
			const std::vector<std::string> d = DifferAll(pRetail);
			const std::string sum = BloodModWeapons_Summary();
			const std::string want = std::string(BloodModWeapon_Name(nId)) + ": " + key + " ";
			// The value reads back as written, through the loader's own formatter: for a float
			// that is the nearest float's shortest form
			const std::string got = BloodModWeapon_Value(nId, f);
			const double dNew = atof(sNew.c_str()), dGot = atof(got.c_str());
			const bool bValue = (sNew[0] == '"' || sNew[0] == 'T' || sNew[0] == 'F') ? got == sNew
							  : sNew[0] == '<' ? got != sRetail
							  : fabs(dGot - dNew) <= 1e-6 * (1.0 + fabs(dNew));
			const bool bOk = bLoaded && d.size() == 1 && sum.compare(0, want.size(), want) == 0 &&
							 sum.find('\n') == std::string::npos && bValue;
			if (bOk)
				reached.insert(d[0].substr(d[0].find('.') + 1));
			else if (++nBad <= 8)
				printf("        [%s] %s = %s: loaded %d, moved %s, value %s, summary \"%s\"%s\n",
					   BloodModWeapon_Name(nId), key, sNew.c_str(), (int)bLoaded, Join(d).c_str(), got.c_str(),
					   sum.c_str(), err.empty() ? "" : (" err " + err).c_str());
		}
	char b[200];
	snprintf(b, sizeof(b), "%d single-field overrides (%d keys x %d weapons): each moves its one member to its value, "
			 "and the summary names it", nCases, nKeys, (int)WEAP_MAXWEAPONTYPES);
	Check(nBad == 0, b);
	std::vector<std::string> unreached;
	for (int m = 0; m < kNumMembers; ++m)
		if (!reached.count(kMembers[m].pName)) unreached.push_back(kMembers[m].pName);
	Check(unreached.size() == 1 && unreached[0] == "m_nType",
		  "every WeaponData member but m_nType is some key's (none: " + Join(unreached) + ")");
	Unload();
}

void TestErrors(const WeaponData* pRetail)
{
	printf("errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a section naming no weapon",	"[BFG9000]\nMinDamage = 1.0\n",			"is not a Blood 2 weapon" },
		{ "a key naming no field",		"[BERETTA]\nDamage = 1.0\n",			"has no field \"Damage\"" },
		{ "m_nType, which is no key",	"[BERETTA]\nType = 2\n",				"has no field \"Type\"" },
		{ "a string for a number",		"[BERETTA]\nMinDamage = \"1.0\"\n",		"is not a number" },
		{ "a number for a string",		"[BERETTA]\nFireSound = 3\n",			"is not a quoted string" },
		{ "a float for an int",			"[BERETTA]\nShotsPerFire = 2.0\n",		"is not an integer" },
		{ "a number for a bool",		"[BERETTA]\nSemiAuto = 1\n",			"is not TRUE or FALSE" },
		{ "a float for a vector",		"[BERETTA]\nRecoil = 1.0\n",			"is not a <x, y, z> vector" },
		{ "a negative float",			"[BERETTA]\nRange = -1.0\n",			"Range is out of range" },
		{ "a negative int",				"[BERETTA]\nDamageRadius = -5\n",		"DamageRadius is out of range" },
		{ "AmmoUse past a D_WORD",		"[BERETTA]\nAmmoUse = 65536\n",			"AmmoUse is out of range" },
		{ "a FireType with no name",	"[BERETTA]\nFireType = \"LASER\"\n",	"must be one of \"RIFLE\"" },
		{ "a FireType as its number",	"[BERETTA]\nFireType = 2\n",			"is not a quoted string" },
		{ "a 100-character string",		0,										"longer than 99 characters" },
		{ "a repeated section",			"[BERETTA]\nRange = 1.0\n[BERETTA]\nRange = 2.0\n", "" },
	};
	const std::string sLong = "[BERETTA]\nViewModel = \"" + std::string(100, 'x') + "\"\n";
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		// A good mod first, so that "loads nothing" is seen to undo it
		LoadWeapons("[SHOTGUN]\nShotsPerFire = 99\n", err);
		const bool bBefore = Differ(pRetail, WEAP_SHOTGUN).size() == 1;
		err.clear();
		const bool bLoaded = LoadWeapons(bad[i].text ? bad[i].text : sLong, err);
		const bool bRetail = DifferAll(pRetail).empty();
		Check(bBefore && !bLoaded && bRetail && !err.empty() && err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	// And the good file next to one bad line is still refused whole
	std::string err;
	const bool bLoaded = LoadWeapons("[SHOTGUN]\nShotsPerFire = 99\n[BERETTA]\nRange = -1.0\n", err);
	Check(!bLoaded && DifferAll(pRetail).empty(), "a good section beside a bad one is not applied");
	// A longest legal string, and an absent file
	Check(LoadWeapons("[BERETTA]\nViewModel = \"" + std::string(99, 'x') + "\"\n", err),
		  "a 99-character string loads");
	{
		MemSource src;
		src.files["Mod\\Mod.txt"] = "[Mod]\nId = \"Test\"\nGame = \"Blood2\"\n";
		LT1Mod::State s;
		LT1Mod::Load(src, "Blood2", s);
		LT1Mod::SetActive(s);
		Check(BloodModWeapons_Load(src, err) && err.empty() && DifferAll(pRetail).empty(),
			  "a mod without Weapons.txt sets the table back to retail, and is no error");
	}
	Unload();
}

void TestStrings(const WeaponData* pRetail)
{
	printf("strings\n");
	std::string err;
	LoadWeapons("[BERETTA]\nFireSound = \"Sounds\\Mod\\first.wav\"\nProjectileClass = \"CTestBullet\"\n", err);
	const char* pFirst = g_WeaponDefaults[WEAP_BERETTA - 1].m_szFireSound;
	const char* pClass = g_WeaponDefaults[WEAP_BERETTA - 1].m_szProjectileClass;
	Check(pFirst && !strcmp(pFirst, "Sounds\\Mod\\first.wav") && pClass && !strcmp(pClass, "CTestBullet"),
		  "a mod's strings are in the table, a backslash as one backslash");
	LoadWeapons("[BERETTA]\nFireSound = \"Sounds\\Mod\\second.wav\"\n", err);
	LoadWeapons("[SHOTGUN]\nFireSound = \"\"\n", err);
	Check(!strcmp(pFirst, "Sounds\\Mod\\first.wav") && !strcmp(pClass, "CTestBullet"),
		  "and outlive two later loads: a weapon made under the first mod keeps its pointers");
	Check(g_WeaponDefaults[WEAP_BERETTA - 1].m_szFireSound == pRetail[WEAP_BERETTA - 1].m_szFireSound,
		  "a load starts from retail: the first mod's sound is gone from the table");
	const char* pEmpty = g_WeaponDefaults[WEAP_SHOTGUN - 1].m_szFireSound;
	Check(pEmpty && !*pEmpty, "FireSound = \"\" is \"\", as retail writes an unused sound");

	// "" is NULL where retail writes NULL
	const int nFlare = WEAP_FLAREGUN, nBeretta = WEAP_BERETTA;
	Check(pRetail[nFlare - 1].m_szProjectileClass && !pRetail[nBeretta - 1].m_szProjectileClass,
		  "retail: the flare gun fires a projectile class, the Beretta none");
	LoadWeapons("[FLAREGUN]\nProjectileClass = \"\"\nStatusIcon = \"\"\n", err);
	Check(!g_WeaponDefaults[nFlare - 1].m_szProjectileClass && !g_WeaponDefaults[nFlare - 1].m_szStatusIcon,
		  "ProjectileClass = \"\" and StatusIcon = \"\" are NULL: the flare gun becomes a vector weapon");
	Check(BloodModWeapons_Summary() == "FLAREGUN: ProjectileClass \"\", StatusIcon \"\"",
		  "and the summary says so: " + BloodModWeapons_Summary());
	Unload();
}

// A whole rebalance, as a mod author would write one, read back
void TestRebalance(const WeaponData* pRetail)
{
	printf("a rebalance\n");
	std::string err;
	const char* text =
		"// Double damage for the pistols; a shotgun with half again the pellets, tighter (retail 16, 200 x 100).\n"
		"[beretta]\nMinDamage = 16.0\nMaxDamage = 24.0\n"
		"[SubMachineGun]\nMinDamage = 16\nMaxDamage = 24.0\n"
		"[SHOTGUN]\nShotsPerFire = 24\nSpreadH = 150.0\nSpreadV = 75.0\n";
	const bool bLoaded = LoadWeapons(text, err);
	const WeaponData& b = g_WeaponDefaults[WEAP_BERETTA - 1];
	const WeaponData& s = g_WeaponDefaults[WEAP_SHOTGUN - 1];
	Check(bLoaded && b.m_fMinDamage == 16.0f && b.m_fMaxDamage == 24.0f &&
		  g_WeaponDefaults[WEAP_SUBMACHINEGUN - 1].m_fMinDamage == 16.0f && s.m_dwShotsPerFire == 24 &&
		  s.m_Spread.h == 150.0f && s.m_Spread.v == 75.0f, "the values are in the table; an int reads as a float" +
		  (err.empty() ? std::string() : ": " + err));
	// BERETTA's two damages, the SMG's two, the shotgun's shots and its Spread (one member, two
	// keys)
	Check(DifferAll(pRetail).size() == 6, "six members moved, no others: " + Join(DifferAll(pRetail)));
	printf("        summary:\n%s\n", BloodModWeapons_Summary().c_str());
	Unload();
}

// The example mod, tests/Blood2Tables/balance_mod, as the shells load it
void TestBalanceMod(const WeaponData* pRetail)
{
	printf("the example mod\n");
	std::string text, err;
	FILE* f = fopen(BLOOD2_BALANCE_MOD, "rb");
	if (f)
	{
		char buf[4096];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
		fclose(f);
	}
	Check(!text.empty(), std::string("read ") + BLOOD2_BALANCE_MOD);
	const bool bLoaded = LoadWeapons(text, err);
	const std::string sum = BloodModWeapons_Summary();
	Check(bLoaded && sum ==
		  "BERETTA: MinDamage 16.0, MaxDamage 24.0, MinAltDamage 20.0, MaxAltDamage 40.0\n"
		  "SHOTGUN: SpreadH 150.0, SpreadV 75.0, ShotsPerFire 24\n"
		  "MINIGUN: StrengthReq 1", "it loads, and the summary the console prints is:\n" + sum +
		  (err.empty() ? std::string() : "\n" + err));
	Check(DifferAll(pRetail).size() == 7 && g_WeaponDefaults[WEAP_MINIGUN - 1].m_dwStrengthReq == 1 &&
		  pRetail[WEAP_MINIGUN - 1].m_dwStrengthReq == 5, "seven members moved (the spread is one), and the Vulcan's strength is 1, retail 5");
	Unload();
}

// New weapons: a section naming no weapon, with a Base

// The members in which two entries of the table differ, by name
std::vector<std::string> DifferEntries(const WeaponData& a, const WeaponData& b)
{
	std::vector<std::string> out;
	for (int m = 0; m < kNumMembers; ++m)
		if (!SameMember(a, b, kMembers[m])) out.push_back(kMembers[m].pName);
	return out;
}

bool AllZero(const WeaponData& w)
{
	const char* p = (const char*)&w;
	for (size_t i = 0; i < sizeof(w); ++i) if (p[i]) return false;
	return true;
}

void TestNewWeapons(const WeaponData* pRetail)
{
	printf("new weapons\n");
	std::string err;
	// WEAP_MAXWEAPONTYPES is an unparenthesized sum: "127 - WEAP_MAXWEAPONTYPES" would be
	// 127 - WEAP_LASTCREATUREWEAPON + 3
	const int kRetail = (WEAP_MAXWEAPONTYPES);
	const int kRoom = BLOOD2_WEAPON_IDS - kRetail;
	const int kFirst = kRetail + 1;

	// Every retail weapon as a Base, with nothing else: the new weapon is its Base on every member,
	// and every retail weapon stays retail's
	int nBad = 0;
	for (int b = 1; b <= WEAP_MAXWEAPONTYPES; ++b)
	{
		const std::string text = std::string("[NewOn") + BloodModWeapon_Name(b) + "]\nBase = \"" + BloodModWeapon_Name(b) + "\"\n";
		const bool bLoaded = LoadWeapons(text, err);
		if (b >= WEAP_BASEINVENTORY)
		{
			// The bombs are inventory items, never made as a weapon: refused
			if ((bLoaded || err.find("a bomb") == std::string::npos || BloodModWeapon_Count() != kRetail ||
				 !AllZero(g_WeaponDefaults[kFirst - 1])) && ++nBad <= 5)
				printf("        Base %s: loaded %d %s\n", BloodModWeapon_Name(b), (int)bLoaded, err.c_str());
			continue;
		}
		const std::vector<std::string> d = DifferEntries(g_WeaponDefaults[kFirst - 1], pRetail[b - 1]);
		bool bKeys = true;
		for (int f = 0; BloodModWeapon_Key(f); ++f)
			if (BloodModWeapon_Value(kFirst, f) != BloodModWeapon_Value(b, f, true)) bKeys = false;
		const bool bOk = bLoaded && BloodModWeapon_Count() == kFirst && BloodModWeapon_Retail(kFirst) == b &&
						 BloodModWeapon_Base(kFirst) == b && d.empty() && bKeys && DifferAll(pRetail).empty() &&
						 BloodModWeapon_Find((std::string("newon") + BloodModWeapon_Name(b)).c_str()) == kFirst;
		if (!bOk && ++nBad <= 5)
			printf("        Base %s: loaded %d, count %d, moved %s %s\n", BloodModWeapon_Name(b), (int)bLoaded,
				   BloodModWeapon_Count(), Join(d).c_str(), err.c_str());
	}
	char b[200];
	snprintf(b, sizeof(b), "a new weapon on each of the %d retail weapons but the bombs is its Base on all %d members, "
			 "numbered %d, and no retail weapon moves; on each of the %d bombs it is refused, and nothing loads",
			 WEAP_BASEINVENTORY - 1, kNumMembers, kFirst, kRetail - WEAP_BASEINVENTORY + 1);
	Check(nBad == 0, b);

	// Each key, on a new weapon, moves that one member of it and nothing else
	int nCases = 0;
	nBad = 0;
	for (int f = 0; BloodModWeapon_Key(f); ++f)
	{
		const char* key = BloodModWeapon_Key(f);
		const std::string sNew = Other(key, BloodModWeapon_Value(WEAP_SHOTGUN, f, true));
		const bool bLoaded = LoadWeapons(std::string("[DemoShotgun]\nBase = \"SHOTGUN\"\n") + key + " = " + sNew + "\n", err);
		const std::vector<std::string> d = DifferEntries(g_WeaponDefaults[kFirst - 1], pRetail[WEAP_SHOTGUN - 1]);
		++nCases;
		if (!(bLoaded && d.size() == 1 && DifferAll(pRetail).empty() &&
			  BloodModWeapons_Summary().find(std::string("DemoShotgun: new weapon ") + std::to_string(kFirst) +
											", Base \"SHOTGUN\", " + key + " ") == 0) && ++nBad <= 5)
			printf("        %s = %s: loaded %d, moved %s, summary %s %s\n", key, sNew.c_str(), (int)bLoaded, Join(d).c_str(),
				   BloodModWeapons_Summary().c_str(), err.c_str());
	}
	snprintf(b, sizeof(b), "%d single-field overrides on a new weapon: each moves its one member, and the summary "
			 "names the weapon, its number and its Base", nCases);
	Check(nBad == 0, b);

	// The mod's own retail overrides reach a new weapon on that Base, whichever order the file
	// gives them; a chain of Bases ends at a retail weapon
	Check(LoadWeapons("[A1]\nBase = \"SHOTGUN\"\nMinDamage = 99.0\n[A2]\nBase = \"A1\"\nRange = 777.0\n"
					  "[SHOTGUN]\nShotsPerFire = 24\n", err) &&
		  BloodModWeapon_Count() == kFirst + 1 && BloodModWeapon_Base(kFirst + 1) == kFirst &&
		  BloodModWeapon_Retail(kFirst + 1) == WEAP_SHOTGUN && g_WeaponDefaults[kFirst].m_dwShotsPerFire == 24 &&
		  g_WeaponDefaults[kFirst].m_fMinDamage == 99.0f && g_WeaponDefaults[kFirst].m_fRange == 777.0f &&
		  g_WeaponDefaults[kFirst - 1].m_fRange == pRetail[WEAP_SHOTGUN - 1].m_fRange,
		  "a new weapon on a new one inherits both, and the mod's retail SHOTGUN override, given after them" +
		  (err.empty() ? std::string() : ": " + err));

	// Strings: a new weapon inherits its Base's mod string, and a string it sets itself replaces
	// that one, "" included, which is NULL in the six
	Check(LoadWeapons("[FLAREGUN]\nProjectileClass = \"CModFlare\"\nFireSound = \"Sounds\\Mod\\flare.wav\"\n"
					  "[Plain]\nBase = \"FLAREGUN\"\n[Vector]\nBase = \"FLAREGUN\"\nProjectileClass = \"\"\nFireSound = \"\"\n", err) &&
		  !strcmp(g_WeaponDefaults[kFirst - 1].m_szProjectileClass, "CModFlare") &&
		  !strcmp(g_WeaponDefaults[kFirst - 1].m_szFireSound, "Sounds\\Mod\\flare.wav") &&
		  !g_WeaponDefaults[kFirst].m_szProjectileClass && !strcmp(g_WeaponDefaults[kFirst].m_szFireSound, "") &&
		  !strcmp(g_WeaponDefaults[WEAP_FLAREGUN - 1].m_szProjectileClass, "CModFlare"),
		  "a new weapon takes its Base's mod strings, and its own replace them, \"\" as NULL where retail has NULL");

	// Bad files, each refused whole over a mod loaded before it
	struct Bad { const char* label; std::string text; const char* expect; };
	std::string many;
	for (int i = 0; i <= kRoom; ++i)
		many += "[Many" + std::to_string(i) + "]\nBase = \"BERETTA\"\n";
	const Bad bad[] =
	{
		{ "a new section with no Base",		"[BFG]\nMinDamage = 1.0\n",			"and has no Base to make it a new one" },
		{ "a Base naming nothing",			"[BFG]\nBase = \"PHASER\"\n",		"Base \"PHASER\" is not a Blood 2 weapon, nor a new one above it" },
		{ "a Base below its new weapon",	"[B1]\nBase = \"B2\"\n[B2]\nBase = \"SHOTGUN\"\n", "Base \"B2\" is not" },
		{ "a Base on a retail weapon",		"[SHOTGUN]\nBase = \"BERETTA\"\n",	"is retail's; Base is only for a new one" },
		{ "a 32-character name",			"[N2345678901234567890123456789012]\nBase = \"SHOTGUN\"\n", "at most 31" },
		{ "a bad key on a new weapon",		"[BFG]\nBase = \"SHOTGUN\"\nDamage = 1.0\n", "has no field \"Damage\"" },
		{ "one new weapon too many",		many,								"one new entry too many: there is room for" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		LoadWeapons("[Kept]\nBase = \"BERETTA\"\n", err);
		const bool bBefore = BloodModWeapon_Count() == kFirst;
		const bool bLoaded = LoadWeapons(bad[i].text, err);
		Check(bBefore && !bLoaded && BloodModWeapon_Count() == WEAP_MAXWEAPONTYPES && DifferAll(pRetail).empty() &&
			  AllZero(g_WeaponDefaults[kFirst - 1]) && err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod with a new weapon: " + err);
	}
	many.clear();
	for (int i = 0; i < kRoom; ++i)
		many += "[Many" + std::to_string(i) + "]\nBase = \"BERETTA\"\n";
	Check(LoadWeapons(many, err) && BloodModWeapon_Count() == BLOOD2_WEAPON_IDS &&
		  BloodModWeapon_Name(BLOOD2_WEAPON_IDS) && BloodModWeapon_Retail(BLOOD2_WEAPON_IDS) == WEAP_BERETTA,
		  std::to_string(kRoom) + " new weapons fit, the last as " +
		  std::to_string(BLOOD2_WEAPON_IDS) + ", under the dual-hand bit (128)");

	// Unloading clears the new weapons' room to the zeros the module started with
	Unload();
	bool bZero = true;
	for (int id = kFirst; id <= BLOOD2_WEAPON_IDS; ++id) bZero = bZero && AllZero(g_WeaponDefaults[id - 1]);
	Check(bZero && BloodModWeapon_Count() == WEAP_MAXWEAPONTYPES && !BloodModWeapon_Name(kFirst) &&
		  BloodModWeapon_Retail(kFirst) == kFirst && BloodModWeapon_Retail(WEAP_SHOTGUN) == WEAP_SHOTGUN,
		  "with no mod: no new weapon, their room is zero, and Retail(id) is the id itself");
}

int SelfTest()
{
	// Retail's table, before anything writes it
	static WeaponData retail[WEAP_MAXWEAPONTYPES];
	memcpy(retail, g_WeaponDefaults, sizeof(retail));
	Check(kNumMembers == 56, "kMembers lists WeaponData's 56 members");
	{
		size_t nCovered = 0;
		for (int m = 0; m < kNumMembers; ++m) nCovered += kMembers[m].nSize;
		char b[128];
		snprintf(b, sizeof(b), "and they cover %u of its %u bytes (the rest padding)", (unsigned)nCovered,
				 (unsigned)sizeof(WeaponData));
		Check(nCovered <= sizeof(WeaponData) && nCovered + 16 > sizeof(WeaponData), b);
	}
	TestNames();
	TestRoundTrip(retail);
	TestMutation(retail);
	TestErrors(retail);
	TestStrings(retail);
	TestRebalance(retail);
	TestBalanceMod(retail);
	TestNewWeapons(retail);
	Check(memcmp(retail, g_WeaponDefaults, sizeof(retail)) == 0, "and the table is byte for byte retail's at the end");
	CharactersSelfTest();
	AISelfTest();
	StringsSelfTest();
	printf("passed %d, failed %d\n", g_nPass, g_nFail);
	return g_nFail ? 1 : 0;
}

} // namespace

// --load-mod <dir>: a mod folder's every table, <dir>/Mod/*.txt, loaded as the shells load them
// (the weapons, characters, AI and commands as the server does, the strings as the client does),
// and the two scripts, each printing the summary the console would, the manifest's line first.
// Exit 1 if the manifest is invalid or any table refuses its file. tools/modtool.py check runs it.
static int LoadMod(const char* pDir)
{
	MemSource src;
	const std::string modDir = std::string(pDir) + "/Mod";
	std::error_code ec;
	for (std::filesystem::directory_iterator it(modDir, ec), end; !ec && it != end; ++it)
		if (it->is_regular_file())
			src.files["Mod\\" + it->path().filename().string()] = Slurp(it->path().string().c_str());
	if (src.files.empty()) { fprintf(stderr, "testBlood2Tables: nothing in %s\n", modDir.c_str()); return 1; }
	LT1Mod::State st;
	LT1Mod::Load(src, "Blood2", st);
	LT1Mod::SetActive(st);
	printf("manifest: %s\n", LT1Mod::Describe(st).c_str());
	if (!st.valid) return 1;

	struct Table { const char* pFile; bool (*pLoad)(LT1Mod::FileSource&, std::string&); std::string (*pSummary)(); };
	const Table kTables[] =
	{
		{ "Mod\\Weapons.txt",		BloodModWeapons_Load,		BloodModWeapons_Summary },
		{ "Mod\\Characters.txt",	BloodModCharacters_Load,	BloodModCharacters_Summary },
		{ "Mod\\AI.txt",			BloodModAI_Load,			BloodModAI_Summary },
		{ "Mod\\Commands.txt",		LT1Mod::StartCommands_Load,	LT1Mod::StartCommands_Summary },
		{ "Mod\\Strings.txt",		BloodModStrings_Load,		BloodModStrings_Summary },
	};
	bool bOk = true;
	for (size_t t = 0; t < sizeof(kTables) / sizeof(kTables[0]); ++t)
	{
		std::string sErr;
		if (!kTables[t].pLoad(src, sErr)) { printf("%s\n", sErr.c_str()); bOk = false; continue; }
		const std::string sum = kTables[t].pSummary();
		for (size_t b = 0, e; b < sum.size(); b = e + 1)
		{
			e = sum.find('\n', b);
			if (e == std::string::npos) e = sum.size();
			if (e > b) printf("%s: %s\n", kTables[t].pFile, sum.substr(b, e - b).c_str());
		}
	}
	// And the scripts, the server's and the client's, with no world
	const auto print = [](const char* p) { printf("%s\n", p); };
	bOk = LT1Mod::Script_Check(src, LT1Mod::kScriptServer, print) && bOk;
	bOk = LT1Mod::Script_Check(src, LT1Mod::kScriptClient, print) && bOk;
	return bOk ? 0 : 1;
}

int main(int argc, char** argv)
{
	// A line at a time, so a crash still leaves the checks before it in the log
	setvbuf(stdout, NULL, _IONBF, 0);	// the UCRT refuses _IOLBF with no size
	// The string module the build put beside this binary, under BLOOD2_GAME_DIR (Games/Blood2, or
	// Games/Blood2NM for the add-on's harness); BLOOD2_CRES overrides it
	{
		std::string dir(argv[0]);
		const size_t slash = dir.find_last_of("/\\");
		dir = slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
#ifdef _WIN32
		g_CResPath = dir + "\\" BLOOD2_GAME_DIR "\\CRes.dll";
#else
		g_CResPath = dir + "/" BLOOD2_GAME_DIR "/libCRes.so";
#endif
		if (getenv("BLOOD2_CRES")) g_CResPath = getenv("BLOOD2_CRES");
	}
	if (argc == 2 && !strcmp(argv[1], "--selftest"))
		return SelfTest();
	if (argc == 3 && !strcmp(argv[1], "--load-mod"))
		return LoadMod(argv[2]);
	const bool bWeapons = argc == 4 && !strcmp(argv[2], "weapons");
	const bool bCharacters = argc == 4 && !strcmp(argv[2], "characters");
	const bool bAI = argc == 4 && !strcmp(argv[2], "ai");
	const bool bStrings = argc == 4 && !strcmp(argv[2], "strings");
	if ((bWeapons || bCharacters || bAI || bStrings) && (!strcmp(argv[1], "--dump") || !strcmp(argv[1], "--check")))
	{
		std::map<int, std::string> retail;
		if (bStrings && !StringsRetail(retail))
		{
			fprintf(stderr, "no string module at %s (BLOOD2_CRES names another)\n", g_CResPath.c_str());
			return 1;
		}
		const std::string text = bWeapons ? Dump() : bCharacters ? CharactersDump() : bAI ? AIDump() : StringsDump(retail);
		if (!strcmp(argv[1], "--dump"))
		{
			FILE* f = fopen(argv[3], "wb");
			if (!f) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
			fwrite(text.data(), 1, text.size(), f);
			fclose(f);
			return 0;
		}
		FILE* f = fopen(argv[3], "rb");
		if (!f) { fprintf(stderr, "cannot read %s\n", argv[3]); return 1; }
		fclose(f);
		const std::string have = Slurp(argv[3]);
		// Without carriage returns: a Windows checkout (core.autocrlf) holds the committed reference
		// CRLF
		std::string lf;
		for (size_t i = 0; i < have.size(); ++i)
			if (have[i] != '\r') lf += have[i];
		if (lf != text) { fprintf(stderr, "%s is stale: testBlood2Tables --dump %s it\n", argv[3], argv[2]); return 1; }
		return 0;
	}
	fprintf(stderr, "usage: testBlood2Tables --selftest | --load-mod <dir> | --dump weapons|characters|ai|strings <out> | --check weapons|characters|ai|strings <file>\n");
	return 2;
}
