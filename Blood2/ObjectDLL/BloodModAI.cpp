// BloodModAI.cpp: see BloodModAI.h

#include "BloodModAI.h"
#include "BloodModTable.h"
#include "BloodModRules.h"		// the OBJFX_ names

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"
#include "SFXMsgIds.h"			// the OBJFX_ names

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{

// Every class whose END_CLASS_DEFAULT names AI_Mgr as its parent.
// GremlinAI and Nightmare are compiled only with _ADD_ON.
const char* const kClasses[] =
{
	"AncientOne", "AncientOneTentacle", "Behemoth", "BirdAI", "BoneLeech", "BugAI", "CalebAI",
	"CivilianAI", "CultistAI", "DeathShroud", "DrudgeLord", "DrudgePriest", "EnhancedGideon",
	"Fanatic", "GabriellaAI", "GabriellaREV", "Gideon",
#ifdef _ADD_ON
	"GremlinAI",
#endif
	"IshmaelAI", "IshmaelREV", "MadScientistAI", "Naga",
#ifdef _ADD_ON
	"Nightmare",
#endif
	"OpheliaAI", "OpheliaREV", "Prophet", "RatAI", "ShikariAI", "SoulDrudge", "TheHandAI",
	"Thief", "UndeadGideon", "WurmAI", "ZealotAI",
};
const int kNumClasses = (int)(sizeof(kClasses) / sizeof(kClasses[0]));

enum Kind { AK_FLOAT, AK_INT, AK_STR, AK_BOOL, AK_FLAGS, AK_PATH, AK_OBJFX, AK_MODE };	// not FLOAT and INT: windef.h has both
struct Field { const char* pKey; size_t nOffset, nSize; Kind eKind; };
#define X(key, member, kind) { #key, offsetof(BloodModAIValues, member), sizeof(((BloodModAIValues*)0)->member), AK_##kind },
const Field kFields[] = { BLOODMOD_AI_FIELDS(X) };
#undef X
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const size_t kMaxString = 31;		// AI_Mgr's char[32]

// The Flags key's names (BloodModAIFields.h).
struct FlagName { const char* pName; DDWORD dwBit; };
#define X(name, bit) { #name, (DDWORD)(bit) },
const FlagName kFlagNames[] = { BLOODMOD_AI_FLAGS(X) };
#undef X
const int kNumFlagNames = (int)(sizeof(kFlagNames) / sizeof(kFlagNames[0]));
const size_t kMaxSkin = BLOODMOD_AI_MAX_SKIN;	// ObjectCreateStruct's m_SkinName, less its terminator
const int kMaxDepth = 16;			// an archetype chain, class included

// The death table's effects are SFXMsgIds.h's OBJFX_ names, a list Mod\Rules.txt reads too:
// BloodMod_ObjFX (Shared/BloodModRules.h)

// [Options]' death table: every class's, under its own keys. Unset is the game's own: a fire
// death takes "skins\enemies\burned.dtx".
#define N(name) #name,
const char* const kDeathKinds[BLOODMOD_DEATH_KINDS] = { BLOODMOD_DEATH_KIND_NAMES(N) };
#undef N
const char* const kRetailDeathSkin[BLOODMOD_DEATH_KINDS] = { "", "skins\\enemies\\burned.dtx", "", "", "", "", "" };
struct DeathOptions
{
	bool		bSkin[BLOODMOD_DEATH_KINDS], bFX[BLOODMOD_DEATH_KINDS];
	std::string	skin[BLOODMOD_DEATH_KINDS], fx[BLOODMOD_DEATH_KINDS];
	bool		bBulletGibSet, bBulletGib;
	void Clear()
	{
		for (int k = 0; k < BLOODMOD_DEATH_KINDS; ++k) { bSkin[k] = bFX[k] = false; skin[k].clear(); fx[k].clear(); }
		bBulletGibSet = false;
		bBulletGib = true;
	}
};
DeathOptions g_Death;

struct Override
{
	bool				bSet[kNumFields];
	BloodModAIValues	v;
	std::string			skin;				// "" when the section gives none
};
// A mod's new enemy type (BloodModAI.h): a section that names no class and
// gives a Base, a class or another archetype.
struct Archetype
{
	std::string			name, base;
	int					nBase;				// its Base's archetype index, -1 when the Base is a class
	int					nClass;				// the class at the root of its chain
	Override			o;
};
std::vector<Override>	g_Overrides;		// by class index, empty when no mod sets any
std::vector<Archetype>	g_Archetypes;
std::vector<bool>		g_Seen;				// classes the trace has printed
bool					g_bTrace = false;
bool					g_bScriptAll = false;	// [Options] Script

// [Options]' difficulty scales (BloodModAI.h): by BloodModAI_DifficultyKind, then easy, medium,
// hard; a kind is set whole or not at all.
const char* const kDifficultyKeys[BLOODMOD_DIFFICULTY_KINDS] = { "DifficultyDamage", "DifficultyHitPoints", "DifficultyArmor" };
bool					g_bDifficulty[BLOODMOD_DIFFICULTY_KINDS] = { false, false, false };
float					g_fDifficulty[BLOODMOD_DIFFICULTY_KINDS][3];

const char* const kPath = "Mod\\AI.txt";

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

// "CRAWL | JUMP" -> the bits; "" (or blanks) is 0. A part that is not a name in the list, an
// empty part, or a bit named twice is an error, and sBad names it.
std::string Trim(const std::string& s)
{
	const size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t");
	return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

bool ParseFlags(const std::string& s, DDWORD& dw, std::string& sBad)
{
	dw = 0;
	if (Trim(s).empty()) return true;
	for (size_t i = 0; ; )
	{
		const size_t j = s.find('|', i);
		const std::string name = Trim(s.substr(i, j == std::string::npos ? std::string::npos : j - i));
		int n = -1;
		for (int k = 0; k < kNumFlagNames && n < 0; ++k)
			if (EqualNoCase(kFlagNames[k].pName, name.c_str())) n = k;
		if (n < 0 || (dw & kFlagNames[n].dwBit))
		{
			sBad = name.empty() ? std::string("an empty part") : name;
			return false;
		}
		dw |= kFlagNames[n].dwBit;
		if (j == std::string::npos) return true;
		i = j + 1;
	}
}

// The bits as names, in the list's order; a bit with no name as hex.
std::string FlagsText(DDWORD dw)
{
	std::string out;
	for (int k = 0; k < kNumFlagNames; ++k)
		if (dw & kFlagNames[k].dwBit)
		{
			out += std::string(out.empty() ? "" : " | ") + kFlagNames[k].pName;
			dw &= ~kFlagNames[k].dwBit;
		}
	if (dw)
	{
		char b[24];
		snprintf(b, sizeof(b), "0x%08x", (unsigned)dw);
		out += std::string(out.empty() ? "" : " | ") + b;
	}
	return out;
}

char* At(BloodModAIValues& v, const Field& f) { return (char*)&v + f.nOffset; }
const char* At(const BloodModAIValues& v, const Field& f) { return (const char*)&v + f.nOffset; }

} // namespace

const char* BloodModAI_Class(int n)
{
	return (n >= 0 && n < kNumClasses) ? kClasses[n] : 0;
}

int BloodModAI_FindClass(const char* pName)
{
	if (pName)
		for (int i = 0; i < kNumClasses; ++i)
			if (EqualNoCase(kClasses[i], pName)) return i;
	return -1;
}

const char* BloodModAI_Key(int nField)
{
	return (nField >= 0 && nField < kNumFields) ? kFields[nField].pKey : 0;
}

std::string BloodModAI_Value(const BloodModAIValues& v, int nField)
{
	if (nField < 0 || nField >= kNumFields) return std::string();
	const Field& f = kFields[nField];
	char b[64];
	switch (f.eKind)
	{
		case AK_FLOAT:	return BloodMod::DF(*(const float*)At(v, f));
		case AK_INT:
		case AK_MODE:	snprintf(b, sizeof(b), "%d", *(const int*)At(v, f)); return b;
		case AK_BOOL:	return *(const DBOOL*)At(v, f) ? "TRUE" : "FALSE";
		case AK_FLAGS:	return "\"" + FlagsText(*(const DDWORD*)At(v, f)) + "\"";
		case AK_STR:
		case AK_OBJFX:
		case AK_PATH:
		{
			// Never read past the member, terminated or not
			const char* p = At(v, f);
			return "\"" + std::string(p, strnlen(p, f.nSize)) + "\"";
		}
	}
	return std::string();
}

void BloodModAI_Clear()
{
	g_Overrides.clear();
	g_Archetypes.clear();
	g_Seen.clear();
	g_bTrace = false;
	g_bScriptAll = false;
	for (int k = 0; k < BLOODMOD_DIFFICULTY_KINDS; ++k) g_bDifficulty[k] = false;
	g_Death.Clear();
}

bool BloodModAI_Load(LT1Mod::FileSource &src, std::string &sError)
{
	BloodModAI_Clear();
	sError.clear();
	if (!LT1Mod::IsActive())
		return true;

	std::string text;
	if (!src.ReadFile(kPath, text))
		return true;							// a mod need not touch the AI

	LT1Mod::Attributes a;
	if (!a.Parse(text.data(), text.size(), kPath))
	{
		sError = a.Error();
		return false;
	}

	// Built in scratch and committed only if the whole file is good
	std::vector<Override> table((size_t)kNumClasses);
	for (size_t i = 0; i < table.size(); ++i)
	{
		memset(table[i].bSet, 0, sizeof(table[i].bSet));
		memset(&table[i].v, 0, sizeof(table[i].v));
	}
	std::vector<Archetype> archetypes;
	bool bTrace = false, bAny = false, bScriptAll = false;
	bool bDifficulty[BLOODMOD_DIFFICULTY_KINDS] = { false, false, false };
	DeathOptions death;
	death.Clear();
	float fDifficulty[BLOODMOD_DIFFICULTY_KINDS][3];
	char buf[512];

	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const std::vector<std::string> keys = a.Keys(tag);
		if (EqualNoCase(tag, "Options"))
		{
			for (size_t k = 0; k < keys.size(); ++k)
			{
				int nKind = -1;
				for (int d = 0; d < BLOODMOD_DIFFICULTY_KINDS; ++d)
					if (EqualNoCase(keys[k].c_str(), kDifficultyKeys[d])) nKind = d;
				if (nKind >= 0)
				{
					// <easy, medium, hard>, each 0 or more.
					float v[3];
					const std::string before = a.Error();
					const bool bVec = a.GetVector(tag, keys[k].c_str(), v);
					if (a.Error() != before || !bVec || !(v[0] >= 0.0f && v[0] <= 1e30f) || !(v[1] >= 0.0f && v[1] <= 1e30f) ||
						!(v[2] >= 0.0f && v[2] <= 1e30f))
					{
						snprintf(buf, sizeof(buf), "%s: [Options]:%s must be three scales, 0 or more, for easy, medium "
								 "and hard: <0.25, 1.0, 1.5>", kPath, kDifficultyKeys[nKind]);
						sError = buf;
						return false;
					}
					bDifficulty[nKind] = true;
					for (int i = 0; i < 3; ++i) fDifficulty[nKind][i] = v[i];
					continue;
				}
				// The death table's every-class keys, and BulletGib
				int nSkin = -1, nFX = -1;
				for (int d = 0; d < BLOODMOD_DEATH_KINDS; ++d)
				{
					if (EqualNoCase(keys[k].c_str(), (std::string("DeathSkin") + kDeathKinds[d]).c_str())) nSkin = d;
					if (EqualNoCase(keys[k].c_str(), (std::string("DeathFX") + kDeathKinds[d]).c_str())) nFX = d;
				}
				if (nSkin >= 0 || nFX >= 0 || EqualNoCase(keys[k].c_str(), "BulletGib"))
				{
					const std::string before = a.Error();
					if (nSkin < 0 && nFX < 0)
					{
						death.bBulletGib = a.GetBool(tag, keys[k].c_str(), true);
						death.bBulletGibSet = true;
					}
					else
					{
						const std::string v = a.GetString(tag, keys[k].c_str(), "");
						if (a.Error() == before && nSkin >= 0 && v.size() > kMaxSkin)
						{
							snprintf(buf, sizeof(buf), "%s: [Options]:%s is longer than %u characters", kPath,
									 keys[k].c_str(), (unsigned)kMaxSkin);
							sError = buf;
							return false;
						}
						if (a.Error() == before && nFX >= 0 && !v.empty() && BloodModAI_ObjFX(v.c_str()) < 0)
						{
							snprintf(buf, sizeof(buf), "%s: [Options]:%s: \"%s\" is not an effect (SFXMsgIds.h's OBJFX_ "
									 "names without the prefix)", kPath, keys[k].c_str(), v.c_str());
							sError = buf;
							return false;
						}
						if (nSkin >= 0) { death.bSkin[nSkin] = true; death.skin[nSkin] = v; }
						else { death.bFX[nFX] = true; death.fx[nFX] = v; }
					}
					if (a.Error() != before)
					{
						sError = std::string(kPath) + ": " + a.Error();
						return false;
					}
					continue;
				}
				const bool bScriptKey = EqualNoCase(keys[k].c_str(), "Script");
				if (!EqualNoCase(keys[k].c_str(), "Trace") && !bScriptKey)
				{
					snprintf(buf, sizeof(buf), "%s: [Options] has no option \"%s\" (Trace, Script, DifficultyDamage, "
							 "DifficultyHitPoints, DifficultyArmor, BulletGib, DeathSkin<Kind>, DeathFX<Kind>)", kPath,
							 keys[k].c_str());
					sError = buf;
					return false;
				}
				const std::string before = a.Error();
				(bScriptKey ? bScriptAll : bTrace) = a.GetBool(tag, keys[k].c_str(), false);
				if (a.Error() != before)
				{
					sError = std::string(kPath) + ": " + a.Error();
					return false;
				}
			}
			continue;
		}
		const int nClass = BloodModAI_FindClass(tag);
		if (nClass >= 0 && a.Has(tag, "Base"))
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is an AI class, so it has no Base", kPath, tag);
			sError = buf;
			return false;
		}
		if (nClass < 0 && !a.Has(tag, "Base"))
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is not a Blood 2 AI class (a class whose parent is AI_Mgr, as a "
					 "level names it; Blood2/ModReference/AI.txt lists them); a new enemy type names its Base, "
					 "a class or another archetype", kPath, tag);
			sError = buf;
			return false;
		}
		if (nClass < 0)
		{
			Archetype n;
			n.name = tag;
			n.nBase = -1;
			n.nClass = -1;
			memset(n.o.bSet, 0, sizeof(n.o.bSet));
			memset(&n.o.v, 0, sizeof(n.o.v));
			archetypes.push_back(n);
		}
		Override& o = nClass >= 0 ? table[(size_t)nClass] : archetypes.back().o;

		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			if (EqualNoCase(key, "Base") || EqualNoCase(key, "Skin"))
			{
				const std::string before = a.Error();
				const std::string v = a.GetString(tag, key, "");
				if (a.Error() != before)
				{
					sError = std::string(kPath) + ": " + a.Error();
					return false;
				}
				if (EqualNoCase(key, "Base"))
				{
					archetypes.back().base = v;
					continue;
				}
				if (v.empty() || v.size() > kMaxSkin)
				{
					snprintf(buf, sizeof(buf), "%s: [%s]:Skin must be a file name of 1 to %u characters", kPath, tag,
							 (unsigned)kMaxSkin);
					sError = buf;
					return false;
				}
				o.skin = v;
				bAny = true;
				continue;
			}
			int nField = -1;
			for (int i = 0; i < kNumFields && nField < 0; ++i)
				if (EqualNoCase(kFields[i].pKey, key)) nField = i;
			if (nField >= 0 && nClass < 0 && EqualNoCase(key, "Cabal"))
			{
				// A save does not keep m_bCabal, and a restore applies it again from the class's section
				// before it knows the archetype (ModAI_ApplyRestored), so an archetype's would be lost
				snprintf(buf, sizeof(buf), "%s: [%s]:Cabal is a class's key: an archetype keeps its class's", kPath, tag);
				sError = buf;
				return false;
			}
			if (nField < 0)
			{
				snprintf(buf, sizeof(buf), "%s: [%s] has no field \"%s\" (Blood2/ModReference/AI.txt lists them all)",
						 kPath, tag, key);
				sError = buf;
				return false;
			}
			const Field& f = kFields[nField];
			const std::string before = a.Error();
			bool bRange = true;
			switch (f.eKind)
			{
				case AK_FLOAT:
				{
					const float x = a.GetFloat(tag, key, 0.0f);
					bRange = x >= 0.0f && x <= 1e30f;		// and not NaN
					*(float*)At(o.v, f) = x;
					break;
				}
				case AK_INT:
				{
					const int n = a.GetInt(tag, key, 0);
					bRange = n >= 0;
					*(int*)At(o.v, f) = n;
					break;
				}
				case AK_BOOL:
				{
					*(DBOOL*)At(o.v, f) = a.GetBool(tag, key, false) ? DTRUE : DFALSE;
					break;
				}
				case AK_FLAGS:
				{
					const std::string s = a.GetString(tag, key, "");
					DDWORD dw = 0;
					std::string sBad;
					if (a.Error() == before && !ParseFlags(s, dw, sBad))
					{
						snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not a flag (AI_Mgr.h's FLAG_ names without "
								 "the prefix, joined by |; Blood2/ModReference/AI.txt lists them)", kPath, tag, key, sBad.c_str());
						sError = buf;
						return false;
					}
					*(DDWORD*)At(o.v, f) = dw;
					break;
				}
				case AK_STR:
				case AK_PATH:
				case AK_OBJFX:
				{
					const std::string s = a.GetString(tag, key, "");
					bRange = s.size() < f.nSize;
					if (f.eKind == AK_OBJFX && a.Error() == before && !s.empty() && BloodModAI_ObjFX(s.c_str()) < 0)
					{
						snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not an effect (SFXMsgIds.h's OBJFX_ names "
								 "without the prefix)", kPath, tag, key, s.c_str());
						sError = buf;
						return false;
					}
					if (bRange)
					{
						memset(At(o.v, f), 0, f.nSize);
						memcpy(At(o.v, f), s.data(), s.size());
					}
					break;
				}
				case AK_MODE:
				{
					const int n = a.GetInt(tag, key, 0);
					bRange = n >= 0 && n <= 2;
					*(int*)At(o.v, f) = n;
					break;
				}
			}
			if (a.Error() != before)
			{
				sError = std::string(kPath) + ": " + a.Error();
				return false;
			}
			if (!bRange)
			{
				if (f.eKind == AK_STR || f.eKind == AK_PATH || f.eKind == AK_OBJFX)
					snprintf(buf, sizeof(buf), "%s: [%s]:%s is longer than %u characters", kPath, tag, key,
							 (unsigned)(f.nSize - 1));
				else if (f.eKind == AK_MODE)
					snprintf(buf, sizeof(buf), "%s: [%s]:%s is 0 (never), 1 (always) or 2 (the game's rule)", kPath, tag,
							 key);
				else
					snprintf(buf, sizeof(buf), "%s: [%s]:%s is out of range", kPath, tag, key);
				sError = buf;
				return false;
			}
			o.bSet[nField] = true;
			bAny = true;
		}
	}

	// Each archetype's Base: a class, or an archetype anywhere in the file,
	// and the chain ends at a class.
	for (size_t i = 0; i < archetypes.size(); ++i)
	{
		Archetype& n = archetypes[i];
		for (size_t j = 0; j < archetypes.size(); ++j)
			if (j != i && EqualNoCase(archetypes[j].name.c_str(), n.base.c_str())) n.nBase = (int)j;
		const int nBaseClass = BloodModAI_FindClass(n.base.c_str());
		if (n.nBase < 0 && nBaseClass < 0)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:Base \"%s\" is neither a Blood 2 AI class nor an archetype in this file",
					 kPath, n.name.c_str(), n.base.c_str());
			sError = buf;
			return false;
		}
	}
	for (size_t i = 0; i < archetypes.size(); ++i)
	{
		int nAt = (int)i, nDepth = 1;
		while (archetypes[(size_t)nAt].nBase >= 0 && nDepth <= kMaxDepth)
		{
			nAt = archetypes[(size_t)nAt].nBase;
			++nDepth;
		}
		if (nDepth > kMaxDepth)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]'s Base chain loops or is deeper than %d", kPath,
					 archetypes[i].name.c_str(), kMaxDepth);
			sError = buf;
			return false;
		}
		archetypes[i].nClass = BloodModAI_FindClass(archetypes[(size_t)nAt].base.c_str());
	}

	if (bAny || !archetypes.empty())
		g_Overrides.swap(table);
	g_Archetypes.swap(archetypes);
	g_Seen.assign((size_t)kNumClasses, false);
	g_bTrace = bTrace;
	g_bScriptAll = bScriptAll;
	g_Death = death;
	for (int d = 0; d < BLOODMOD_DIFFICULTY_KINDS; ++d)
	{
		g_bDifficulty[d] = bDifficulty[d];
		for (int i = 0; i < 3 && bDifficulty[d]; ++i) g_fDifficulty[d][i] = fDifficulty[d][i];
	}
	return true;
}

float BloodModAI_DifficultyScale(int nKind, int nDifficulty, float fRetail)
{
	if (nKind < 0 || nKind >= BLOODMOD_DIFFICULTY_KINDS || nDifficulty < 0 || nDifficulty > 2 || !g_bDifficulty[nKind])
		return fRetail;
	return g_fDifficulty[nKind][nDifficulty];
}

namespace
{
int ApplyOverride(const Override& o, BloodModAIValues& v)
{
	int n = 0;
	for (int i = 0; i < kNumFields; ++i)
	{
		if (!o.bSet[i]) continue;
		const Field& f = kFields[i];
		memcpy(At(v, f), At(o.v, f), f.nSize);
		++n;
	}
	return n;
}
} // namespace

int BloodModAI_Apply(const char* pClass, BloodModAIValues& v)
{
	const int nClass = BloodModAI_FindClass(pClass);
	if (nClass < 0 || g_Overrides.empty()) return 0;
	return ApplyOverride(g_Overrides[(size_t)nClass], v);
}

int BloodModAI_FindArchetype(const char* pName)
{
	if (pName)
		for (size_t i = 0; i < g_Archetypes.size(); ++i)
			if (EqualNoCase(g_Archetypes[i].name.c_str(), pName)) return (int)i;
	return -1;
}

const char* BloodModAI_ArchetypeName(int nArch)
{
	return (nArch >= 0 && (size_t)nArch < g_Archetypes.size()) ? g_Archetypes[(size_t)nArch].name.c_str() : 0;
}

const char* BloodModAI_ArchetypeClass(int nArch)
{
	return (nArch >= 0 && (size_t)nArch < g_Archetypes.size()) ? kClasses[g_Archetypes[(size_t)nArch].nClass] : 0;
}

int BloodModAI_ApplyArchetype(int nArch, BloodModAIValues& v)
{
	if (nArch < 0 || (size_t)nArch >= g_Archetypes.size()) return 0;
	// Root first, so each archetype's own keys replace its Base's.
	std::vector<int> chain;
	for (int n = nArch; n >= 0; n = g_Archetypes[(size_t)n].nBase) chain.push_back(n);
	int nKeys = 0;
	for (size_t i = chain.size(); i-- > 0; )
		nKeys += ApplyOverride(g_Archetypes[(size_t)chain[i]].o, v);
	return nKeys;
}

const char* BloodModAI_Skin(const char* pClass, int nArch)
{
	for (int n = nArch; n >= 0 && (size_t)n < g_Archetypes.size(); n = g_Archetypes[(size_t)n].nBase)
		if (!g_Archetypes[(size_t)n].o.skin.empty()) return g_Archetypes[(size_t)n].o.skin.c_str();
	const int nClass = BloodModAI_FindClass(pClass);
	if (nClass >= 0 && (size_t)nClass < g_Overrides.size() && !g_Overrides[(size_t)nClass].skin.empty())
		return g_Overrides[(size_t)nClass].skin.c_str();
	return 0;
}

bool BloodModAI_Trace()
{
	return g_bTrace;
}

bool BloodModAI_ScriptAll()
{
	return g_bScriptAll;
}

const char* BloodModAI_ObjFXName(int nId)	{ return BloodMod_ObjFXName(nId); }
int BloodModAI_ObjFX(const char* pName)		{ return BloodMod_ObjFX(pName); }

const char* BloodModAI_DeathSkin(int nKind)
{
	if (nKind < 0 || nKind >= BLOODMOD_DEATH_KINDS) return "";
	return g_Death.bSkin[nKind] ? g_Death.skin[nKind].c_str() : kRetailDeathSkin[nKind];
}

const char* BloodModAI_DeathFX(int nKind)
{
	if (nKind < 0 || nKind >= BLOODMOD_DEATH_KINDS || !g_Death.bFX[nKind]) return "";
	return g_Death.fx[nKind].c_str();
}

bool BloodModAI_BulletGib()
{
	return g_Death.bBulletGib;
}

bool BloodModAI_FirstOfClass(const char* pClass)
{
	const int nClass = BloodModAI_FindClass(pClass);
	if (nClass < 0 || (size_t)nClass >= g_Seen.size() || g_Seen[(size_t)nClass]) return false;
	g_Seen[(size_t)nClass] = true;
	return true;
}

namespace
{
std::string SummaryLine(const Override& o)
{
	std::string line;
	for (int i = 0; i < kNumFields; ++i)
		if (o.bSet[i])
			line += std::string(line.empty() ? " " : ", ") + kFields[i].pKey + " " + BloodModAI_Value(o.v, i);
	if (!o.skin.empty())
		line += std::string(line.empty() ? " " : ", ") + "Skin \"" + o.skin + "\"";
	return line;
}
} // namespace

std::string BloodModAI_Summary()
{
	std::string out;
	for (size_t c = 0; c < g_Overrides.size(); ++c)
	{
		const std::string line = SummaryLine(g_Overrides[c]);
		if (!line.empty())
			out += (out.empty() ? "" : "\n") + std::string(kClasses[c]) + ":" + line;
	}
	for (size_t i = 0; i < g_Archetypes.size(); ++i)
	{
		const std::string line = SummaryLine(g_Archetypes[i].o);
		out += (out.empty() ? "" : "\n") + g_Archetypes[i].name + ": new archetype, Base " + g_Archetypes[i].base +
			   (g_Archetypes[i].nBase >= 0 ? std::string(" (on ") + kClasses[g_Archetypes[i].nClass] + ")" : std::string()) +
			   (line.empty() ? std::string() : "," + line);
	}
	for (int d = 0; d < BLOODMOD_DIFFICULTY_KINDS; ++d)
		if (g_bDifficulty[d])
			out += std::string(out.empty() ? "" : "\n") + kDifficultyKeys[d] + " <" + BloodMod::DF(g_fDifficulty[d][0]) + ", " +
				   BloodMod::DF(g_fDifficulty[d][1]) + ", " + BloodMod::DF(g_fDifficulty[d][2]) + ">";
	for (int d = 0; d < BLOODMOD_DEATH_KINDS; ++d)
	{
		if (g_Death.bSkin[d])
			out += std::string(out.empty() ? "" : "\n") + "DeathSkin" + kDeathKinds[d] + " \"" + g_Death.skin[d] + "\"";
		if (g_Death.bFX[d])
			out += std::string(out.empty() ? "" : "\n") + "DeathFX" + kDeathKinds[d] + " \"" + g_Death.fx[d] + "\"";
	}
	if (g_Death.bBulletGibSet)
		out += std::string(out.empty() ? "" : "\n") + "BulletGib " + (g_Death.bBulletGib ? "TRUE" : "FALSE");
	if (g_bScriptAll)
		out += std::string(out.empty() ? "" : "\n") + "Script on, every class";
	if (g_bTrace)
		out += std::string(out.empty() ? "" : "\n") + "Trace on";
	return out;
}

std::string BloodModAI_Reference()
{
	std::string out = "\n// The keys.  FLOAT and INT are 0 or more; a STR is at most 31 characters;\n"
					  "// a BOOL is TRUE or FALSE; FLAGS is a string of the names below joined by\n"
					  "// |, the whole word (\"CRAWL | JUMP\", \"\" for none).\n//\n";
	static const char* const kKind[] = { "  FLOAT", "  INT", "  STR", "  BOOL", "  FLAGS", "  PATH", "  OBJFX", "  MODE" };
	for (int i = 0; i < kNumFields; ++i)
		out += std::string("//     ") + kFields[i].pKey + kKind[kFields[i].eKind] + "\n";
	out += "//     Skin  STR, a skin file of at most " + std::to_string(kMaxSkin) + " characters, which replaces\n"
		   "//           the one the class picks\n"
		   "//\n"
		   "// A section with any other name is a new enemy type, an archetype, and names\n"
		   "// its Base: a class below, or another archetype.  It takes the same keys.  An\n"
		   "// AI of the Base's class with the property Archetype = \"<name>\" is one.\n"
		   "//\n"
		   "//\n"
		   "// Behavior (BestWeapon to SpawnAlways):\n"
		   "//     BestWeapon     FALSE takes the attack states' change to the best weapon\n"
		   "//                    out (CultistAI, Fanatic, Prophet, GabriellaAI/REV,\n"
		   "//                    OpheliaAI/REV have one)\n"
		   "//     ChaseRun       TRUE: AI_STATE_AttackFar closes in at a run, not a walk\n"
		   "//     AttackWeapon   a weapon, as AIWeapon1 names one, taken up on entering an\n"
		   "//                    attack state\n"
		   "//     NoAltFire      a weapon Prophet never alt-fires standing still\n"
		   "//     ChaseNear      DeathShroud: TRUE closes in only while already within a\n"
		   "//                    third of SeeingDist, the test turned round\n"
		   "//     FadeMax, FadeMin  DeathShroud's alpha, solid and faded (0.9, 0.5)\n"
		   "//     LoopVolume     DeathShroud's and DrudgePriest's looping sound (50, 60)\n"
		   "//     SpawnCooldown, SpawnInterval  DrudgePriest: seconds between summons\n"
		   "//                    (30) and between leeches in one (1.5)\n"
		   "//     SpawnAlways    DrudgePriest: TRUE summons with a leech already in sight\n"
		   "//     Robe           CultistAI: TRUE puts every cultist in the robe (its model,\n"
		   "//                    skin, animations and sounds), whatever the level says\n"
		   "//     DeathSkin<Kind>, DeathFX<Kind>  the corpse's skin, and the effect on the\n"
		   "//                    dying AI, for a death by that damage: Kind is Normal,\n"
		   "//                    Fire, Acid, Electric, Explode, Singularity or Melee; an\n"
		   "//                    effect is an OBJFX_ name without the prefix (SMOKING_3)\n"
		   "//     DeathAnimRandom  TRUE: a death that takes the class's DeathSkin picks a\n"
		   "//                    random death animation\n"
		   "//     DropOnDeath    a class made at the corpse's torso by a death of one of the\n"
		   "//                    kinds above (ZealotAI: at its leg, as it vanishes);\n"
		   "//                    DropOnDeathDivine, a divine ZealotAI's\n"
		   "//     CorpseLimbLoss, GibLimbs  MODE: whether the corpse can lose limbs, and\n"
		   "//                    whether a gibbing throws them; the game's rule (2) is a\n"
		   "//                    Cabal AI or a SoulDrudge\n"
		   "//     GibHead, GibRightArm, GibLeftArm, GibRightLeg, GibLeftLeg  a gib throws\n"
		   "//                    that limb when a roll of 0-100 is above it (80 70 50 70 50)\n"
		   "//     BurstSmokeNode  Fanatic: the model node its last charge's smoke source\n"
		   "//                    is made at (torso_extra1); it is attached to the torso\n"
		   "//                    the next update\n"
		   "//     BurstRange     Fanatic: how close its last charge gets before it bursts\n"
		   "//                    (100); it bursts after 30 s whatever the range\n"
		   "//     BurstProjectile  Fanatic: the shell the burst drops, a projectile class\n"
		   "//                    (CHowitzerShell); \"\" drops none\n"
		   "//     BurstSpawn     Fanatic: a class the burst leaves instead of the shell,\n"
		   "//                    BurstSpawnChance percent of the time (\"\", 100), with\n"
		   "//                    gibs of BurstSpawnGibDamage (50) and one of\n"
		   "//                    BurstSpawnSound1-3 at random, where it stood\n"
		   "//     Script         TRUE: the mod's Script.lua hears every AI of the class\n"
		   "//                    (OnCreate, OnDamage, OnDeath, OnTrigger), not only its\n"
		   "//                    archetypes\n"
		   "//\n"
		   "// [Options] takes Trace and Script (TRUE or FALSE; Script is every class's)\n"
		   "// and three difficulty scales, each\n"
		   "// <easy, medium, hard> (insane has none, as in the game):\n"
		   "//     DifficultyDamage     the damage an AI deals (retail <0.25, 1.0, 1.5>)\n"
		   "//     DifficultyHitPoints  its starting hit points (retail <0.25, 1.0, 1.5>)\n"
		   "//     DifficultyArmor      its armor (retail <0.25, 1.0, 1.5>)\n"
		   "// and the death table for every class, before a class's own keys:\n"
		   "//     DeathSkin<Kind>, DeathFX<Kind>  as above (retail: DeathSkinFire is\n"
		   "//                          \"skins\\enemies\\burned.dtx\", with a random animation)\n"
		   "//     BulletGib            FALSE: only an explosion (or ALWAYSGIB) bursts a body,\n"
		   "//                          not a 90% hit\n"
		   "//\n"
		   "// The Flags names (AI_Mgr.h's FLAG_):";
	for (int k = 0; k < kNumFlagNames; ++k)
		out += std::string(k % 6 ? " " : "\n//    ") + kFlagNames[k].pName;
	out += "\n";
	out += "//\n// The classes, one section each.\n\n";
	for (int c = 0; c < kNumClasses; ++c)
		out += std::string("[") + kClasses[c] + "]\n";
	return out;
}

void BloodModAI_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModAI_Load(src, sErr);
	if (!pPrint) return;
	BloodMod::PrintWrapped(bOk ? BloodModAI_Summary() : sErr,
						   std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(kPath) + ": " : std::string()), pPrint);
}
