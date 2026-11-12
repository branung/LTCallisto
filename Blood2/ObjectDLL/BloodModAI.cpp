// BloodModAI.cpp: see BloodModAI.h

#include "BloodModAI.h"
#include "BloodModTable.h"

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"

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

enum Kind { AK_FLOAT, AK_INT, AK_STR };	// not FLOAT and INT: windef.h has both
struct Field { const char* pKey; size_t nOffset; Kind eKind; };
#define X(key, member, kind) { #key, offsetof(BloodModAIValues, member), AK_##kind },
const Field kFields[] = { BLOODMOD_AI_FIELDS(X) };
#undef X
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const size_t kMaxString = 31;		// AI_Mgr's char[32]
const size_t kMaxSkin = BLOODMOD_AI_MAX_SKIN;	// ObjectCreateStruct's m_SkinName, less its terminator
const int kMaxDepth = 16;			// an archetype chain, class included

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

const char* const kPath = "Mod\\AI.txt";

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

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
		case AK_INT:	snprintf(b, sizeof(b), "%d", *(const int*)At(v, f)); return b;
		case AK_STR:
		{
			// Never read past the member, terminated or not
			const char* p = At(v, f);
			return "\"" + std::string(p, strnlen(p, kMaxString + 1)) + "\"";
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
	bool bTrace = false, bAny = false;
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
				if (!EqualNoCase(keys[k].c_str(), "Trace"))
				{
					snprintf(buf, sizeof(buf), "%s: [Options] has no option \"%s\" (only Trace)", kPath, keys[k].c_str());
					sError = buf;
					return false;
				}
				const std::string before = a.Error();
				bTrace = a.GetBool(tag, keys[k].c_str(), false);
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
				case AK_STR:
				{
					const std::string s = a.GetString(tag, key, "");
					bRange = s.size() <= kMaxString;
					if (bRange)
					{
						memset(At(o.v, f), 0, kMaxString + 1);
						memcpy(At(o.v, f), s.data(), s.size());
					}
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
				if (f.eKind == AK_STR)
					snprintf(buf, sizeof(buf), "%s: [%s]:%s is longer than %u characters", kPath, tag, key,
							 (unsigned)kMaxString);
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
	return true;
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
		const size_t nSize = f.eKind == AK_STR ? kMaxString + 1 : (f.eKind == AK_INT ? sizeof(int) : sizeof(float));
		memcpy(At(v, f), At(o.v, f), nSize);
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
	if (g_bTrace)
		out += std::string(out.empty() ? "" : "\n") + "Trace on";
	return out;
}

std::string BloodModAI_Reference()
{
	std::string out = "\n// The keys.  FLOAT and INT are 0 or more; a STR is at most 31 characters.\n//\n";
	for (int i = 0; i < kNumFields; ++i)
		out += std::string("//     ") + kFields[i].pKey +
			   (kFields[i].eKind == AK_FLOAT ? "  FLOAT" : kFields[i].eKind == AK_INT ? "  INT" : "  STR") + "\n";
	out += "//     Skin  STR, a skin file of at most " + std::to_string(kMaxSkin) + " characters, which replaces\n"
		   "//           the one the class picks\n"
		   "//\n"
		   "// A section with any other name is a new enemy type, an archetype, and names\n"
		   "// its Base: a class below, or another archetype.  It takes the same keys.  An\n"
		   "// AI of the Base's class with the property Archetype = \"<name>\" is one.\n";
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
