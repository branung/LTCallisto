// ai.cpp: testBlood2Tables for Mod\AI.txt (ObjectDLL/BloodModAI.cpp).
// Not a table: the mod's values are applied to each AI as AI_Mgr creates it,
// so the checks apply them to a BloodModAIValues instead of reading an array.
//
//   classes   the class list is the classes ObjectDLL registers with AI_Mgr
//             as their parent, read from the source (add-on ones need _ADD_ON).
//   members   every member of BloodModAIValues is exactly one key's.
//   mutation  each key, set alone, moves its one member in its own class only.
//   errors    a bad file loads nothing, over a mod loaded before it.
//   strings   at most 31 characters, terminated in the member.
//   trace     [Options] Trace, and the first-of-class answer, once.

#include "BloodModAI.h"
#include "harness.h"

#include <filesystem>
#include <ctype.h>
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

bool LoadAI(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\AI.txt", text, err, BloodModAI_Load);
}

void Unload()
{
	BloodModAI_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// BloodModAIValues' members, transcribed independently of the key list.
struct Member { const char* pName; size_t nOffset, nSize; };
#define M(m) { #m, offsetof(BloodModAIValues, m), sizeof(((BloodModAIValues*)0)->m) }
#define W(n) { "m_szAIWeapon[" #n "]", offsetof(BloodModAIValues, m_szAIWeapon) + (n) * 32, 32 }
#define DS(n) { "m_szModDeathSkin[" #n "]", offsetof(BloodModAIValues, m_szModDeathSkin) + (n) * (BLOODMOD_AI_MAX_SKIN + 1), BLOODMOD_AI_MAX_SKIN + 1 }
#define DF(n) { "m_szModDeathFX[" #n "]", offsetof(BloodModAIValues, m_szModDeathFX) + (n) * 32, 32 }
#define GL(n) { "m_nModGibLimb[" #n "]", offsetof(BloodModAIValues, m_nModGibLimb) + (n) * sizeof(int), sizeof(int) }
#define BS(n) { "m_szModBurstSound[" #n "]", offsetof(BloodModAIValues, m_szModBurstSound) + (n) * (BLOODMOD_AI_MAX_SKIN + 1), BLOODMOD_AI_MAX_SKIN + 1 }
const Member kMembers[] =
{
	M(m_fHearingDist), M(m_fSensingDist), M(m_fSmellingDist), M(m_fSeeingDist),
	M(m_fWalkSpeed), M(m_fRunSpeed), M(m_fJumpSpeed), M(m_fRollSpeed),
	M(m_fAIMass), M(m_fAIHitPoints), M(m_fAIRandomHP), M(m_fAIArmorPoints), M(m_nAIStrength),
	M(m_szAIState), W(0), W(1), W(2), W(3), W(4),
	M(m_fAIBullets), M(m_fAIBMG), M(m_fAIShells), M(m_fAIGrenades), M(m_fAIRockets),
	M(m_fAIFlares), M(m_fAICells), M(m_fAICharges), M(m_fAIFuel), M(m_fProxBombs),
	M(m_dwFlags), M(m_bCabal),
	M(m_bModBestWeapon), M(m_bModChaseRun), M(m_bModChaseNear), M(m_szModAttackWeapon), M(m_szModNoAltFire),
	M(m_fModFadeMax), M(m_fModFadeMin), M(m_nModLoopVolume), M(m_fModSpawnCooldown), M(m_fModSpawnInterval),
	M(m_bModSpawnAlways), M(m_bModScript),
	M(m_bModRobe), DS(0), DS(1), DS(2), DS(3), DS(4), DS(5), DS(6), DF(0), DF(1), DF(2), DF(3), DF(4), DF(5), DF(6),
	M(m_bModDeathAnimRandom), M(m_szModDropOnDeath), M(m_szModDropDivine), M(m_nModCorpseLimbLoss), M(m_nModGibLimbs),
	GL(0), GL(1), GL(2), GL(3), GL(4),
	M(m_szModBurstSmokeNode), M(m_fModBurstRange), M(m_szModBurstProjectile), M(m_szModBurstSpawn), M(m_nModBurstSpawnChance),
	M(m_fModBurstSpawnGibDamage), BS(0), BS(1), BS(2),
};
#undef M
#undef W
#undef DS
#undef DF
#undef GL
#undef BS
const int kNumMembers = (int)(sizeof(kMembers) / sizeof(kMembers[0]));

// A struct no mod value equals: every byte 0x5a, but each string "sentinel"
// and terminated, so that it reads as a value a mod could write.
BloodModAIValues Sentinel()
{
	BloodModAIValues v;
	memset(&v, 0x5a, sizeof(v));
	strcpy(v.m_szAIState, "sentinel");
	for (int i = 0; i < 5; ++i) strcpy(v.m_szAIWeapon[i], "sentinel");
	strcpy(v.m_szModAttackWeapon, "sentinel");
	strcpy(v.m_szModNoAltFire, "sentinel");
	for (int k = 0; k < BLOODMOD_DEATH_KINDS; ++k) { strcpy(v.m_szModDeathSkin[k], "sentinel"); strcpy(v.m_szModDeathFX[k], "sentinel"); }
	strcpy(v.m_szModDropOnDeath, "sentinel");
	strcpy(v.m_szModDropDivine, "sentinel");
	strcpy(v.m_szModBurstSmokeNode, "sentinel");
	strcpy(v.m_szModBurstProjectile, "sentinel");
	strcpy(v.m_szModBurstSpawn, "sentinel");
	for (int k = 0; k < 3; ++k) strcpy(v.m_szModBurstSound[k], "sentinel");
	return v;
}

std::vector<std::string> Differ(const BloodModAIValues& a, const BloodModAIValues& b)
{
	std::vector<std::string> out;
	for (int m = 0; m < kNumMembers; ++m)
		if (memcmp((const char*)&a + kMembers[m].nOffset, (const char*)&b + kMembers[m].nOffset, kMembers[m].nSize))
			out.push_back(kMembers[m].pName);
	return out;
}

int NumKeys()
{
	int n = 0;
	while (BloodModAI_Key(n)) ++n;
	return n;
}

} // namespace

// The classes, read from ObjectDLL's source: every END_CLASS_DEFAULT or
// END_CLASS_DEFAULT_FLAGS whose parent is pParent (AI_Mgr or CProjectile), outside an #ifdef _ADD_ON
// unless this build defines it.  Other conditionals count as taken.
std::set<std::string> ScanClasses(const char* pDir, std::string& sWhy, const char* pParent)
{
	std::set<std::string> out;
	std::error_code ec;
	std::filesystem::directory_iterator it(pDir, ec), end;
	if (ec) { sWhy = std::string("cannot open ") + pDir; return out; }
#ifdef _ADD_ON
	const bool bAddOn = true;
#else
	const bool bAddOn = false;
#endif
	for (; it != end; it.increment(ec))
	{
		if (ec) break;
		// .cpp in any case: ObjectDLL has AI_Shared.CPP.
		std::string ext = it->path().extension().string();
		for (size_t i = 0; i < ext.size(); ++i) ext[i] = (char)tolower((unsigned char)ext[i]);
		if (ext != ".cpp") continue;
		const std::string text = Slurp(it->path().string().c_str());
		std::vector<int> stack;		// 1 taken, 0 not, -1 an _ADD_ON block's #else not taken
		size_t at = 0;
		while (at < text.size())
		{
			size_t eol = text.find('\n', at);
			if (eol == std::string::npos) eol = text.size();
			std::string line = text.substr(at, eol - at);
			at = eol + 1;
			size_t p = line.find_first_not_of(" \t");
			if (p == std::string::npos) continue;
			line = line.substr(p);
			bool bLive = true;
			for (size_t i = 0; i < stack.size(); ++i) if (stack[i] <= 0) bLive = false;
			if (line.compare(0, 3, "#if") == 0)
			{
				if (line.compare(0, 6, "#ifdef") == 0 && line.find("_ADD_ON") != std::string::npos)
					stack.push_back(bAddOn ? 1 : 0);
				else if (line.compare(0, 7, "#ifndef") == 0 && line.find("_ADD_ON") != std::string::npos)
					stack.push_back(bAddOn ? 0 : 1);
				else
					stack.push_back(2);				// not ours: taken, and its #else too
				continue;
			}
			if (line.compare(0, 5, "#else") == 0 && !stack.empty())
			{
				if (stack.back() == 1) stack.back() = 0;
				else if (stack.back() == 0) stack.back() = 1;
				continue;
			}
			if (line.compare(0, 6, "#endif") == 0 && !stack.empty()) { stack.pop_back(); continue; }
			if (!bLive || line.compare(0, 17, "END_CLASS_DEFAULT") != 0) continue;
			const size_t open = line.find('(');
			if (open == std::string::npos) continue;
			char cls[128], parent[128];
			if (sscanf(line.c_str() + open + 1, " %127[A-Za-z0-9_] , %127[A-Za-z0-9_]", cls, parent) == 2 &&
				!strcmp(parent, pParent))
				out.insert(cls);
		}
	}
	return out;
}

namespace
{

// ---------------------------------------------------------------------------

void TestClasses()
{
	printf("AI: classes\n");
	std::string why;
	const std::set<std::string> scanned = ScanClasses(BLOOD2_OBJECTDLL_DIR, why, "AI_Mgr");
	std::set<std::string> listed;
	for (int i = 0; BloodModAI_Class(i); ++i) listed.insert(BloodModAI_Class(i));
	std::vector<std::string> missing, extra;
	for (std::set<std::string>::const_iterator it = scanned.begin(); it != scanned.end(); ++it)
		if (!listed.count(*it)) missing.push_back(*it);
	for (std::set<std::string>::const_iterator it = listed.begin(); it != listed.end(); ++it)
		if (!scanned.count(*it)) extra.push_back(*it);
	char b[256];
	snprintf(b, sizeof(b), "the %d classes listed are the %d ObjectDLL registers with AI_Mgr as their parent "
			 "(missing: %s; extra: %s)%s", (int)listed.size(), (int)scanned.size(), Join(missing).c_str(),
			 Join(extra).c_str(), why.c_str());
	Check(!scanned.empty() && missing.empty() && extra.empty(), b);
#ifdef _ADD_ON
	Check(listed.size() == 34 && listed.count("GremlinAI") && listed.count("Nightmare"),
		  "34 with the Nightmare Levels, GremlinAI and Nightmare among them");
#else
	Check(listed.size() == 32 && !listed.count("GremlinAI") && !listed.count("Nightmare"),
		  "32 in the base game, without the add-on's GremlinAI and Nightmare");
#endif
	Check(BloodModAI_FindClass("cultistai") == BloodModAI_FindClass("CultistAI") && BloodModAI_FindClass("CultistAI") >= 0 &&
		  BloodModAI_FindClass("Cultist") < 0 && BloodModAI_FindClass("AI_Mgr") < 0,
		  "names are matched without case and whole; AI_Mgr itself is not one");
}

void TestMembers()
{
	printf("AI: members\n");
	std::set<std::string> reached;
	int nBad = 0;
	const int nKeys = NumKeys();
	for (int f = 0; f < nKeys; ++f)
	{
		// Each key, set alone, moves exactly one member.
		BloodModAIValues v = Sentinel();
		const BloodModAIValues s = v;
		std::string err;
		const std::string key = BloodModAI_Key(f);
		const std::string sent = BloodModAI_Value(s, f);
		const std::string val = key.compare(0, 7, "DeathFX") == 0 ? "\"SMOKING_3\"" :
								key == "CorpseLimbLoss" || key == "GibLimbs" ? "1" :
								sent[0] == '"' ? (key == "Flags" ? "\"NEVERGIB\"" : "\"X\"") :
								sent == "TRUE" || sent == "FALSE" ? "FALSE" :
								sent.find('.') == std::string::npos ? "3" : "7.5";
		LoadAI("[CultistAI]\n" + key + " = " + val + "\n", err);
		BloodModAI_Apply("CultistAI", v);
		const std::vector<std::string> d = Differ(v, s);
		if (d.size() != 1 || !reached.insert(d[0]).second) { ++nBad; printf("        %s: %s\n", key.c_str(), Join(d).c_str()); }
	}
	char b[160];
	snprintf(b, sizeof(b), "each of the %d keys moves its own member, and no two the same one", nKeys);
	Check(nBad == 0 && nKeys == 77, b);
	Check((int)reached.size() == kNumMembers, "every member of BloodModAIValues is some key's");
	size_t nCovered = 0;
	for (int m = 0; m < kNumMembers; ++m) nCovered += kMembers[m].nSize;
	Check(nCovered == sizeof(BloodModAIValues), "and the members cover the struct, with no padding");
	Unload();
}

void TestMutation()
{
	printf("AI: mutation\n");
	int nCases = 0, nBad = 0;
	const int nKeys = NumKeys();
	for (int c = 0; BloodModAI_Class(c); ++c)
		for (int f = 0; f < nKeys; ++f)
		{
			const char* cls = BloodModAI_Class(c);
			const char* other = BloodModAI_Class(c ? 0 : 1);
			const BloodModAIValues s = Sentinel();
			const std::string sNew = Other(BloodModAI_Key(f), BloodModAI_Value(s, f));
			std::string err;
			++nCases;
			const bool bLoaded = LoadAI(std::string("[") + cls + "]\n" + BloodModAI_Key(f) + " = " + sNew + "\n", err);
			BloodModAIValues v = s, w = s;
			const int n = BloodModAI_Apply(cls, v);
			const int nOther = BloodModAI_Apply(other, w);
			const std::string got = BloodModAI_Value(v, f);
			const double dNew = atof(sNew.c_str()), dGot = atof(got.c_str());
			const bool bValue = sNew[0] == '"' ? got == sNew : fabs(dGot - dNew) <= 1e-6 * (1.0 + fabs(dNew));
			const std::string sum = BloodModAI_Summary();
			const std::string want = std::string(cls) + ": " + BloodModAI_Key(f) + " ";
			if (!(bLoaded && n == 1 && nOther == 0 && Differ(v, s).size() == 1 && Differ(w, s).empty() && bValue &&
				  sum.compare(0, want.size(), want) == 0 && sum.find('\n') == std::string::npos) && ++nBad <= 8)
				printf("        [%s] %s = %s: loaded %d, applied %d/%d, value %s, summary \"%s\" %s\n", cls,
					   BloodModAI_Key(f), sNew.c_str(), (int)bLoaded, n, nOther, got.c_str(), sum.c_str(), err.c_str());
		}
	char b[200];
	snprintf(b, sizeof(b), "%d single-field overrides (%d keys x %d classes): each moves its one member to its value, "
			 "for its class only", nCases, nKeys, nCases / (nKeys ? nKeys : 1));
	Check(nBad == 0, b);
	Unload();
}

void TestErrors()
{
	printf("AI: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a section naming no class",		"[Cultist]\nHitPoints = 1.0\n",			"is not a Blood 2 AI class" },
		{ "AI_Mgr, the base",				"[AI_Mgr]\nHitPoints = 1.0\n",			"is not a Blood 2 AI class" },
		{ "a key naming no member",			"[CultistAI]\nHealth = 1.0\n",			"has no field \"Health\"" },
		{ "a negative float",				"[CultistAI]\nRunSpeed = -1.0\n",		"RunSpeed is out of range" },
		{ "a negative strength",			"[CultistAI]\nStrength = -1\n",			"Strength is out of range" },
		{ "a float for the strength",		"[CultistAI]\nStrength = 2.5\n",		"is not an integer" },
		{ "a number for a weapon",			"[CultistAI]\nAIWeapon1 = 3\n",			"is not a quoted string" },
		{ "a 32-character weapon",			"[CultistAI]\nAIWeapon1 = \"SHOTGUNSHOTGUNSHOTGUNSHOTGUNSHOT\"\n", "longer than 31 characters" },
		{ "an unknown option",				"[Options]\nVerbose = TRUE\n",			"has no option \"Verbose\"" },
		{ "a Trace that is not a bool",		"[Options]\nTrace = 1\n",				"is not TRUE or FALSE" },
		// New enemy types.
		{ "a new section with no Base",		"[RedCultist]\nHitPoints = 1.0\n",		"names its Base" },
		{ "a Base on a class",				"[CultistAI]\nBase = \"ShikariAI\"\n",	"is an AI class, so it has no Base" },
		{ "a Base that is nothing",			"[RedCultist]\nBase = \"Cultist\"\n",		"is neither a Blood 2 AI class nor an archetype" },
		{ "a Base chain that loops",		"[A1]\nBase = \"A2\"\n[A2]\nBase = \"A1\"\n", "Base chain loops" },
		{ "a Base that is itself",			"[A1]\nBase = \"A1\"\n",				"is neither a Blood 2 AI class nor an archetype" },
		{ "an empty Skin",					"[CultistAI]\nSkin = \"\"\n",			"Skin must be a file name" },
		{ "a 100-character Skin",			"[CultistAI]\nSkin = \"SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS\"\n",
											"Skin must be a file name" },
		{ "a number for a Skin",			"[CultistAI]\nSkin = 3\n",				"is not a quoted string" },
		// Flags and Cabal
		{ "a flag that is not one",			"[CultistAI]\nFlags = \"CRAWL | FLY\"\n",	"\"FLY\" is not a flag" },
		{ "a flag with its prefix",			"[CultistAI]\nFlags = \"FLAG_CRAWL\"\n",	"\"FLAG_CRAWL\" is not a flag" },
		{ "a flag named twice",				"[CultistAI]\nFlags = \"JUMP | jump\"\n",	"\"jump\" is not a flag" },
		{ "an empty part",					"[CultistAI]\nFlags = \"JUMP |\"\n",		"an empty part" },
		{ "a number for the flags",			"[CultistAI]\nFlags = 2048\n",			"is not a quoted string" },
		{ "a Cabal that is not a bool",		"[CultistAI]\nCabal = 1\n",				"is not TRUE or FALSE" },
		{ "a Cabal on an archetype",		"[RedCultist]\nBase = \"CultistAI\"\nCabal = FALSE\n", "Cabal is a class's key" },
		// Behavior and the difficulty
		{ "a BestWeapon that is not a bool", "[CultistAI]\nBestWeapon = 0\n",		"is not TRUE or FALSE" },
		{ "a negative LoopVolume",			"[DeathShroud]\nLoopVolume = -1\n",	"LoopVolume is out of range" },
		{ "a 32-character AttackWeapon",	"[Prophet]\nAttackWeapon = \"COMBATCOMBATCOMBATCOMBATCOMBATCO\"\n", "longer than 31 characters" },
		{ "a difficulty of one number",		"[Options]\nDifficultyDamage = 0.5\n",	"DifficultyDamage must be three scales" },
		{ "a negative difficulty scale",	"[Options]\nDifficultyArmor = <0.25, -1.0, 1.0>\n", "DifficultyArmor must be three scales" },
		{ "a difficulty that is a string",	"[Options]\nDifficultyHitPoints = \"0.5\"\n", "DifficultyHitPoints must be three scales" },
		// Script
		{ "a class Script that is not a bool", "[CultistAI]\nScript = 1\n",		"is not TRUE or FALSE" },
		{ "an [Options] Script that is not a bool", "[Options]\nScript = \"yes\"\n", "is not TRUE or FALSE" },
		// The death table
		{ "an effect that is none",			"[CultistAI]\nDeathFXAcid = \"SMOKY\"\n",	"\"SMOKY\" is not an effect" },
		{ "an effect with its prefix",		"[Options]\nDeathFXElectric = \"OBJFX_ELECTRIC_1\"\n", "is not an effect" },
		{ "a mode of 3",					"[CultistAI]\nCorpseLimbLoss = 3\n",	"is 0 (never), 1 (always) or 2" },
		{ "a 100-character death skin",		"[CultistAI]\nDeathSkinFire = \"SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS\"\n",
											"longer than 99 characters" },
		{ "a BulletGib that is not a bool",	"[Options]\nBulletGib = 0\n",			"is not TRUE or FALSE" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadAI("[ShikariAI]\nHitPoints = 5.0\n", err);
		BloodModAIValues v = Sentinel();
		const bool bBefore = BloodModAI_Apply("ShikariAI", v) == 1;
		err.clear();
		const bool bLoaded = LoadAI(bad[i].text, err);
		v = Sentinel();
		Check(bBefore && !bLoaded && BloodModAI_Apply("ShikariAI", v) == 0 && BloodModAI_Summary().empty() &&
			  err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	std::string err;
	Check(MountAndLoad(0, "", err, BloodModAI_Load) && err.empty() && BloodModAI_Summary().empty(),
		  "a mod without AI.txt sets nothing, and is no error");
	Unload();
}

// The Flags key, against known words: AI_Mgr.h's values for the names, and what Extra Crispy's
// constructors set
void TestFlags()
{
	printf("AI: flags\n");
	struct Case { const char* text; DDWORD dw; const char* back; };
	const Case cases[] =
	{
		{ "\"\"",													0x000,	"\"\"" },
		{ "\"  \"",													0x000,	"\"\"" },
		{ "\"NEVERGIB\"",											0x800,	"\"NEVERGIB\"" },
		{ "\"nevergib\"",											0x800,	"\"NEVERGIB\"" },
		{ "\"ALWAYSCLEAR | NEVERGIB\"",								0x900,	"\"ALWAYSCLEAR | NEVERGIB\"" },
		// EC's CultistAI: CRAWL | JUMP | LIMP | DODGE | ALWAYSRECOIL.
		{ "\"CRAWL|JUMP|LIMP|DODGE|ALWAYSRECOIL\"",					0x02f,	"\"CRAWL | JUMP | LIMP | DODGE | ALWAYSRECOIL\"" },
		// Written out of order, read as the same word, written back in the list's.
		{ "\"NEVERGIB | CRAWL\"",									0x801,	"\"CRAWL | NEVERGIB\"" },
		{ "\"CRAWL | JUMP | LIMP | DODGE | LIMBLOSS | ALWAYSRECOIL | ALWAYSGIB | CANASSIST | ALWAYSCLEAR | "
		  "NOAMMOCOLLECT | NIGHTMAREDEATH | NEVERGIB\"",				0xfff,	DNULL },
	};
	int nBad = 0;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		std::string err;
		BloodModAIValues v = Sentinel();
		const bool bLoaded = LoadAI(std::string("[CultistAI]\nFlags = ") + cases[i].text + "\n", err);
		BloodModAI_Apply("CultistAI", v);
		int f = 0;
		while (BloodModAI_Key(f) && strcmp(BloodModAI_Key(f), "Flags")) ++f;
		const std::string back = BloodModAI_Value(v, f);
		if (!bLoaded || v.m_dwFlags != cases[i].dw || (cases[i].back && back != cases[i].back))
		{
			++nBad;
			printf("        %s: loaded %d, 0x%03x, \"%s\" %s\n", cases[i].text, (int)bLoaded, (unsigned)v.m_dwFlags,
				   back.c_str(), err.c_str());
		}
	}
	Check(nBad == 0, "Flags reads each name as AI_Mgr.h's bit, in any case and order, and writes the word back in the "
		  "list's order");
	BloodModAIValues v = Sentinel();
	v.m_dwFlags = 0x1801;
	int f = 0;
	while (BloodModAI_Key(f) && strcmp(BloodModAI_Key(f), "Flags")) ++f;
	Check(BloodModAI_Value(v, f) == "\"CRAWL | NEVERGIB | 0x00001000\"", "a bit with no name is written as hex, not dropped");
	Unload();
}

void TestStrings()
{
	printf("AI: strings\n");
	std::string err;
	const std::string s31(31, 'W');
	BloodModAIValues v = Sentinel();
	Check(LoadAI("[CultistAI]\nAIWeapon2 = \"" + s31 + "\"\nAIState = \"GUARD\"\n", err) &&
		  BloodModAI_Apply("CultistAI", v) == 2, "a 31-character weapon loads" + (err.empty() ? std::string() : ": " + err));
	Check(!memcmp(v.m_szAIWeapon[1], s31.data(), 31) && v.m_szAIWeapon[1][31] == 0 && !strcmp(v.m_szAIState, "GUARD") &&
		  v.m_szAIState[31] == 0, "and is terminated in its member, as is a short one, padded with NULs");
	Check(!strcmp(v.m_szAIWeapon[2], "sentinel"), "the next weapon slot is untouched");
	Unload();
}

void TestDifficulty()
{
	printf("AI: difficulty\n");
	std::string err;
	Unload();
	Check(BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, 1, 1.0f) == 1.0f &&
		  BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_ARMOR, 2, 1.5f) == 1.5f, "with no mod each scale is the game's");
	Check(LoadAI("[Options]\nDifficultyDamage = <0.25, 0.5, 1.0>\nDifficultyHitPoints = <0.1, 0.2, 0.3>\n", err),
		  "Extra Crispy's damage scales and another hit-point set load" + (err.empty() ? std::string() : ": " + err));
	Check(BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, 0, 9.0f) == 0.25f &&
		  BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, 1, 9.0f) == 0.5f &&
		  BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, 2, 9.0f) == 1.0f &&
		  BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_HITPOINTS, 1, 9.0f) == 0.2f,
		  "each difficulty answers its own component, easy medium hard");
	Check(BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_ARMOR, 1, 1.0f) == 1.0f,
		  "a kind the mod does not name keeps the game's scale");
	Check(BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, 3, 1.0f) == 1.0f &&
		  BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, -1, 1.0f) == 1.0f,
		  "insane, which the game scales by nothing, and nonsense answer the caller's value");
	Check(BloodModAI_Summary() == "DifficultyDamage <0.25, 0.5, 1.0>\nDifficultyHitPoints <0.1, 0.2, 0.3>",
		  "the summary names both:\n" + BloodModAI_Summary());
	Unload();
	Check(BloodModAI_DifficultyScale(BLOODMOD_DIFFICULTY_DAMAGE, 1, 1.0f) == 1.0f, "unloading restores the game's");

	printf("AI: Script\n");
	Check(!BloodModAI_ScriptAll(), "with no mod no class is heard by a script");
	Check(LoadAI("[Options]\nScript = TRUE\n", err) && BloodModAI_ScriptAll() && BloodModAI_Summary() == "Script on, every class",
		  "[Options] Script = TRUE opts every class in, and the summary says so: " + BloodModAI_Summary());
	BloodModAIValues v = Sentinel();
	Check(LoadAI("[Fanatic]\nScript = TRUE\n", err) && !BloodModAI_ScriptAll() && BloodModAI_Apply("Fanatic", v) == 1 &&
		  v.m_bModScript && BloodModAI_Apply("CultistAI", v) == 0,
		  "a class's Script = TRUE opts in that class alone");
	Unload();
	Check(!BloodModAI_ScriptAll(), "and unloading takes it away");

	printf("AI: the death table\n");
	Check(std::string(BloodModAI_DeathSkin(1)) == "skins\\enemies\\burned.dtx" && !BloodModAI_DeathSkin(2)[0] &&
		  !BloodModAI_DeathFX(2)[0] && BloodModAI_BulletGib(),
		  "with no mod the table is the game's: a fire death burns, nothing else, and a 90% hit gibs");
	Check(BloodModAI_ObjFX("smoking_3") == 22 && BloodModAI_ObjFX("ELECTRIC_1") == 39 && BloodModAI_ObjFX("") == -1 &&
		  BloodModAI_ObjFX("OBJFX_SMOKING_3") == -1, "effects by SFXMsgIds.h's names, any case, without the prefix");
	Check(LoadAI("[Options]\nDeathSkinAcid = \"skins\\enemies\\burned_acid.dtx\"\nDeathFXAcid = \"SMOKING_3\"\n"
				 "DeathSkinFire = \"\"\nBulletGib = FALSE\n", err),
		  "Extra Crispy's every-class acid line, no fire skin and no bullet gibs load" + (err.empty() ? std::string() : ": " + err));
	Check(std::string(BloodModAI_DeathSkin(2)) == "skins\\enemies\\burned_acid.dtx" && std::string(BloodModAI_DeathFX(2)) == "SMOKING_3" &&
		  !BloodModAI_DeathSkin(1)[0] && !BloodModAI_BulletGib() && BloodModAI_DeathSkin(-1)[0] == 0 && BloodModAI_DeathSkin(7)[0] == 0,
		  "each answers the mod's, an empty fire skin turns the game's off, and a kind out of range is none");
	Unload();
	Check(std::string(BloodModAI_DeathSkin(1)) == "skins\\enemies\\burned.dtx" && BloodModAI_BulletGib(),
		  "and unloading brings the game's back");
}

void TestTrace()
{
	printf("AI: trace\n");
	std::string err;
	Check(LoadAI("[Options]\nTrace = TRUE\n", err) && BloodModAI_Trace() && BloodModAI_Summary() == "Trace on",
		  "[Options] Trace = TRUE turns it on, with nothing set");
	Check(BloodModAI_FirstOfClass("ZealotAI") && !BloodModAI_FirstOfClass("ZealotAI") && BloodModAI_FirstOfClass("zealotai") == false &&
		  BloodModAI_FirstOfClass("CultistAI") && !BloodModAI_FirstOfClass("NotAClass"),
		  "the first of each class answers once, without case; a name that is no class never");
	LoadAI("[CultistAI]\nHitPoints = 1.0\n", err);
	Check(!BloodModAI_Trace() && BloodModAI_FirstOfClass("ZealotAI"), "a later load without it turns it off and starts the classes again");
	Unload();
}

std::string P(const char* p) { return p ? std::string(p) : std::string("<null>"); }

// New enemy types: an archetype on a class, one on an
// archetype, the order each applies in, and the skin.
void TestArchetypes()
{
	printf("AI: new enemy types\n");
	std::string err;
	// The leader first: a Base may come later in the file.
	const bool bLoaded = LoadAI("[RedLeader]\nBase = \"redcultist\"\nRunSpeed = 300.0\n"
								"[CultistAI]\nHitPoints = 50.0\nWalkSpeed = 80.0\n"
								"[RedCultist]\nBase = \"CultistAI\"\nHitPoints = 120.0\nAIWeapon1 = \"SHOTGUN\"\n"
								"Skin = \"Skins\\Enemies\\m_Cultist3.dtx\"\n"
								"[Plain]\nBase = \"ShikariAI\"\n", err);
	Check(bLoaded && BloodModAI_FindArchetype("redleader") == 0 && BloodModAI_FindArchetype("RedCultist") == 1 &&
		  BloodModAI_FindArchetype("Plain") == 2 && BloodModAI_FindArchetype("CultistAI") == -1 &&
		  BloodModAI_FindArchetype("Nothing") == -1 && P(BloodModAI_ArchetypeClass(0)) == "CultistAI" &&
		  P(BloodModAI_ArchetypeClass(2)) == "ShikariAI" && P(BloodModAI_ArchetypeName(1)) == "RedCultist" &&
		  !BloodModAI_ArchetypeName(3),
		  "three archetypes load, found without case, each with the class at the root of its chain" + (bLoaded ? std::string() : ": " + err));

	// What AI_Mgr::ModAI_Apply does for a CultistAI with Archetype RedLeader:
	// the class's section, then the chain root first.
	BloodModAIValues v = Sentinel(), sentinel = Sentinel();
	const int nClass = BloodModAI_Apply("CultistAI", v);
	const int nArch = BloodModAI_ApplyArchetype(0, v);
	Check(nClass == 2 && nArch == 3 && v.m_fAIHitPoints == 120.0f && v.m_fWalkSpeed == 80.0f && v.m_fRunSpeed == 300.0f &&
		  !strcmp(v.m_szAIWeapon[0], "SHOTGUN") && v.m_fSeeingDist == sentinel.m_fSeeingDist &&
		  !strcmp(v.m_szAIWeapon[1], sentinel.m_szAIWeapon[1]),
		  "an archetype's values follow its class's, its Base's first: HitPoints 120 over 50, the class's WalkSpeed kept, "
		  "the leader's RunSpeed, nothing else touched");
	v = Sentinel();
	Check(BloodModAI_ApplyArchetype(2, v) == 0 && !memcmp(&v, &sentinel, sizeof(v)) && BloodModAI_ApplyArchetype(3, v) == 0 &&
		  BloodModAI_ApplyArchetype(-1, v) == 0,
		  "an archetype with only a Base sets nothing, and no index sets nothing");
	Check(P(BloodModAI_Skin("CultistAI", 0)) == "Skins\\Enemies\\m_Cultist3.dtx" &&
		  P(BloodModAI_Skin("CultistAI", 1)) == "Skins\\Enemies\\m_Cultist3.dtx" && !BloodModAI_Skin("CultistAI", -1) &&
		  !BloodModAI_Skin("ShikariAI", 2),
		  "the skin is the chain's nearest; a plain CultistAI and an archetype without one have none");
	const std::string sum = BloodModAI_Summary();
	Check(sum == "CultistAI: WalkSpeed 80.0, HitPoints 50.0\n"
				 "RedLeader: new archetype, Base redcultist (on CultistAI), RunSpeed 300.0\n"
				 "RedCultist: new archetype, Base CultistAI, HitPoints 120.0, AIWeapon1 \"SHOTGUN\", Skin \"Skins\\Enemies\\m_Cultist3.dtx\"\n"
				 "Plain: new archetype, Base ShikariAI",
		  "the summary names each archetype, its Base and the class it is built on:\n" + sum);

	// A class section's own skin, for every AI of the class.
	Check(LoadAI("[ShikariAI]\nSkin = \"Skins\\Enemies\\Shikari2.dtx\"\n[Pale]\nBase = \"ShikariAI\"\n", err) &&
		  P(BloodModAI_Skin("ShikariAI", -1)) == "Skins\\Enemies\\Shikari2.dtx" &&
		  P(BloodModAI_Skin("ShikariAI", 0)) == "Skins\\Enemies\\Shikari2.dtx" && !BloodModAI_Skin("CultistAI", -1) &&
		  BloodModAI_Summary() == "ShikariAI: Skin \"Skins\\Enemies\\Shikari2.dtx\"\nPale: new archetype, Base ShikariAI",
		  "a class section's Skin is every AI's of that class, an archetype's included");
	Unload();
	Check(BloodModAI_FindArchetype("Pale") == -1 && !BloodModAI_Skin("ShikariAI", -1), "unloading removes them");
}

// The example mod, tests/Blood2Tables/ai_mod.
void TestAIMod()
{
	printf("AI: the example mod\n");
	std::string err;
	const std::string text = Slurp(BLOOD2_AI_MOD);
	Check(!text.empty(), std::string("read ") + BLOOD2_AI_MOD);
	const bool bLoaded = LoadAI(text, err);
	const std::string sum = BloodModAI_Summary();
	Check(bLoaded && sum == "CultistAI: HitPoints 1.0, RandomHitPoints 5.0\nTrace on",
		  "it loads, and the summary the console prints is:\n" + sum + (err.empty() ? std::string() : "\n" + err));
	Unload();
}

void TestReference()
{
	printf("AI: the reference\n");
	std::string err;
	Check(LoadAI(AIDump(), err) && BloodModAI_Summary().empty(),
		  "the reference loads as a mod and sets nothing" + (err.empty() ? std::string() : ": " + err));
	Unload();
}

} // namespace

std::string AIDump()
{
	return
		"// Blood II's AI classes and the constants a mod's Mod\\AI.txt can set for\n"
		"// them.  GENERATED by `testBlood2Tables --dump ai` from\n"
		"// Blood2/ObjectDLL/BloodModAI.cpp and BloodModAIFields.h; do not edit,\n"
		"// regenerate.\n"
		"//\n"
		"// A mod names only what it changes:\n"
		"//\n"
		"//     [CultistAI]\n"
		"//     HitPoints = 60.0\n"
		"//\n"
		"// Each class's own values are its constructor's (CultistAI.cpp's\n"
		"// CultistAI::CultistAI), not listed here: the classes are not a table.\n"
		"// A mod with [Options] Trace = TRUE prints them, for each class the\n"
		"// level creates.  A level's own property for the same member (the\n"
		"// properties a class declares in its BEGIN_CLASS) still wins over the\n"
		"// mod's value. Blood2/ObjectDLL/BloodModAI.h has the rest.\n"
		+ BloodModAI_Reference();
}

void AISelfTest()
{
	Check(kNumMembers == 77, "kMembers lists BloodModAIValues' 77 members");
	TestClasses();
	TestMembers();
	TestMutation();
	TestErrors();
	TestStrings();
	TestFlags();
	TestDifficulty();
	TestTrace();
	TestArchetypes();
	TestAIMod();
	TestReference();
}
