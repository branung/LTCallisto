// BloodModObjectFX.cpp: see BloodModObjectFX.h

#include "server_de.h"
#include "SFXMsgIds.h"			// the OBJFX_ ids
#include "BloodModObjectFX.h"
#include "BloodModRules.h"		// BloodMod_ObjFX, the OBJFX_ names
#include "BloodModTable.h"		// PrintWrapped, DF, Table::kMaxString

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

namespace
{

const char* const kPath = "Mod\\ObjectFX.txt";
bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }
const char* const kLevels[3] = { "_LOW", "_MEDIUM", "_HIGH" };

enum Kind { OK_NUMBER, OK_SIGNED, OK_VECTOR, OK_COUNT, OK_EMIT, OK_RAMP, OK_FLAGS, OK_PATH };
struct FieldInfo { const char* pName; Kind eKind; };
// In BloodModObjFXField's order.
const FieldInfo kFields[BMO_FIELDS] =
{
	{ "Offset", OK_VECTOR }, { "RampUpTime", OK_NUMBER }, { "RampDownTime", OK_NUMBER }, { "Duration", OK_NUMBER },
	{ "AddDelay", OK_NUMBER }, { "Radius", OK_NUMBER }, { "Gravity", OK_SIGNED }, { "Rotations", OK_VECTOR },
	{ "PosRadius", OK_NUMBER }, { "EmitType", OK_EMIT }, { "NumParticles", OK_COUNT }, { "Density", OK_NUMBER },
	{ "Alpha", OK_NUMBER }, { "MinColor", OK_VECTOR }, { "MaxColor", OK_VECTOR }, { "MinVelocity", OK_SIGNED },
	{ "MaxVelocity", OK_SIGNED }, { "MinLifetime", OK_NUMBER }, { "MaxLifetime", OK_NUMBER }, { "RampUpType", OK_RAMP },
	{ "RampDownType", OK_RAMP }, { "FXFlags", OK_FLAGS }, { "Particle", OK_PATH },
};

// ObjectFX.h's values, which the client checks against these
// (ObjectFX.cpp's static_asserts): Shared cannot include a client header.
struct Bit { const char* pName; unsigned int nValue; };
const Bit kEmit[] =
{
	{ "LOCATION", 0 }, { "RING", 1 }, { "SPHERE", 2 }, { "CYLINDER", 3 }, { "FILLEDRING", 4 }, { "FILLEDSPHERE", 5 },
	{ "FILLEDCYLINDER", 6 }, { "COMETTAIL", 7 }, { "POWERRING", 8 },
};
const Bit kRamp[] =
{
	{ "ALPHA", 0x01 }, { "VELOCITY", 0x02 }, { "NUMPARTICLES", 0x04 }, { "ROTATION", 0x08 }, { "POSRADIUS", 0x10 },
};
// The composites after the bits, so a word is written with the bits.
const Bit kFlags[] =
{
	{ "REMOVESTOPPED", 0x01 }, { "RAMPDOWNSTOPPED", 0x02 }, { "MOVINGSOURCE", 0x04 }, { "MOVINGSTREAM", 0x08 },
	{ "ROTATINGSOURCE", 0x10 }, { "ALIGNROTATION", 0x20 },
	{ "SMOOTHSTOP", 0x03 }, { "STREAMING", 0x0C }, { "FREEROTATION", 0x30 }, { "ALL", 0xFF },
};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

std::map<int, BloodModObjFXRecipe> g_Recipes;		// key id * 4 + level

int Level(std::string& name)
{
	for (int l = 0; l < 3; ++l)
	{
		const size_t n = strlen(kLevels[l]);
		if (name.size() > n)
		{
			std::string tail = name.substr(name.size() - n);
			for (size_t i = 0; i < tail.size(); ++i) tail[i] = (char)toupper((unsigned char)tail[i]);
			if (tail == kLevels[l]) { name.resize(name.size() - n); return l; }
		}
	}
	return -1;
}

std::string Trim(const std::string& s)
{
	size_t a = 0, b = s.size();
	while (a < b && isspace((unsigned char)s[a])) ++a;
	while (b > a && isspace((unsigned char)s[b - 1])) --b;
	return s.substr(a, b - a);
}

// A number, or a..b.
bool ParseRange(const std::string& text, float& lo, float& hi)
{
	const std::string t = Trim(text);
	if (t.empty()) return false;
	const size_t dd = t.find("..", 1);
	char* e = 0;
	if (dd == std::string::npos)
	{
		lo = hi = (float)strtod(t.c_str(), &e);
		return e && !*e;
	}
	const std::string a = Trim(t.substr(0, dd)), b = Trim(t.substr(dd + 2));
	lo = (float)strtod(a.c_str(), &e);
	if (!e || *e || a.empty()) return false;
	hi = (float)strtod(b.c_str(), &e);
	return e && !*e && !b.empty();
}

// <a, b, c>, each part a number or a range.
bool ParseVectorRange(const std::string& text, float lo[3], float hi[3])
{
	std::string t = Trim(text);
	if (t.size() < 2 || t[0] != '<' || t[t.size() - 1] != '>') return false;
	t = t.substr(1, t.size() - 2);
	for (int i = 0; i < 3; ++i)
	{
		const size_t comma = t.find(',');
		if ((comma == std::string::npos) != (i == 2)) return false;
		if (!ParseRange(i == 2 ? t : t.substr(0, comma), lo[i], hi[i])) return false;
		if (i < 2) t = t.substr(comma + 1);
	}
	return true;
}

std::string RangeText(float lo, float hi)
{
	return lo == hi ? BloodMod::DF(lo) : BloodMod::DF(lo) + ".." + BloodMod::DF(hi);
}

// "A | B" by names out of bits; "" is 0.  False on a name not in bits.
bool ParseWord(const std::string& text, const Bit* bits, int nBits, unsigned int& out, std::string& sBad)
{
	out = 0;
	std::string rest = text;
	while (!Trim(rest).empty())
	{
		const size_t bar = rest.find('|');
		const std::string word = Trim(bar == std::string::npos ? rest : rest.substr(0, bar));
		rest = bar == std::string::npos ? std::string() : rest.substr(bar + 1);
		int i = 0;
		while (i < nBits && !EqualNoCase(bits[i].pName, word.c_str())) ++i;
		if (i == nBits || word.empty()) { sBad = word; return false; }
		out |= bits[i].nValue;
	}
	return true;
}

// A word as names: the bits first, then any composite the bits miss.
std::string WordText(unsigned int n, const Bit* bits, int nBits)
{
	if (n == 0xFF)
		for (int i = 0; i < nBits; ++i)
			if (bits[i].nValue == 0xFF) return bits[i].pName;
	std::string out;
	unsigned int left = n;
	for (int i = 0; i < nBits; ++i)
	{
		const unsigned int v = bits[i].nValue;
		if (v && (v & (v - 1)) == 0 && (left & v))
		{
			out += std::string(out.empty() ? "" : " | ") + bits[i].pName;
			left &= ~v;
		}
	}
	if (left)
	{
		char b[16];
		snprintf(b, sizeof(b), "0x%x", left);
		out += std::string(out.empty() ? "" : " | ") + b;
	}
	return out;
}

} // namespace

const char* BloodModObjectFX_FieldName(int nField)	{ return nField >= 0 && nField < BMO_FIELDS ? kFields[nField].pName : 0; }

const BloodModObjFXRecipe* BloodModObjectFX_Recipe(int nId, int nDetail)
{
	if (nDetail < 0 || nDetail > 2) return 0;
	std::map<int, BloodModObjFXRecipe>::const_iterator it = g_Recipes.find(nId * 4 + nDetail);
	return it == g_Recipes.end() ? 0 : &it->second;
}

std::string BloodModObjectFX_Value(const BloodModObjFXRecipe& r, int nField)
{
	if (nField < 0 || nField >= BMO_FIELDS) return std::string();
	switch (kFields[nField].eKind)
	{
		case OK_NUMBER:
		case OK_SIGNED:
		{
			// A range is text, as the file must quote it.
			const std::string t = RangeText(r.lo[nField][0], r.hi[nField][0]);
			return r.lo[nField][0] == r.hi[nField][0] ? t : "\"" + t + "\"";
		}
		case OK_VECTOR:
		{
			const float* lo = r.lo[nField];
			const float* hi = r.hi[nField];
			const std::string t = "<" + RangeText(lo[0], hi[0]) + ", " + RangeText(lo[1], hi[1]) + ", " +
								  RangeText(lo[2], hi[2]) + ">";
			return lo[0] == hi[0] && lo[1] == hi[1] && lo[2] == hi[2] ? t : "\"" + t + "\"";
		}
		case OK_COUNT:
		{
			char b[16];
			snprintf(b, sizeof(b), "%u", r.n[nField]);
			return b;
		}
		case OK_EMIT:
			for (int i = 0; i < COUNT(kEmit); ++i)
				if (kEmit[i].nValue == r.n[nField]) return std::string("\"") + kEmit[i].pName + "\"";
			return "\"?\"";
		case OK_RAMP:	return "\"" + WordText(r.n[nField], kRamp, COUNT(kRamp)) + "\"";
		case OK_FLAGS:	return "\"" + WordText(r.n[nField], kFlags, COUNT(kFlags)) + "\"";
		case OK_PATH:	return "\"" + r.sParticle + "\"";
	}
	return std::string();
}

void BloodModObjectFX_Clear()
{
	g_Recipes.clear();
}

bool BloodModObjectFX_Load(LT1Mod::FileSource &src, std::string &sError)
{
	BloodModObjectFX_Clear();
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

	char buf[600];
	std::map<int, BloodModObjFXRecipe> recipes;
	std::map<int, int> scope;		// id -> -1 all levels, or a bit per level a section names
	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		std::string name = tags[t];
		const int nLevel = Level(name);
		const int nId = BloodMod_ObjFX(name.c_str());
		if (nId < 0)
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is not a Blood 2 object effect (an OBJFX_ name without the prefix, "
					 "with _HIGH, _MEDIUM or _LOW for one detail level; Blood2/ModReference/ObjectFX.txt lists them)",
					 kPath, tag);
			sError = buf;
			return false;
		}
		int& s = scope[nId];
		if ((nLevel < 0 && s) || (nLevel >= 0 && (s == -1 || (s & (1 << nLevel)))))
		{
			snprintf(buf, sizeof(buf), "%s: [%s]: %s is given twice for a detail level (a section for all three and "
					 "one for a level, or two)", kPath, tag, name.c_str());
			sError = buf;
			return false;
		}
		s = nLevel < 0 ? -1 : (s | (1 << nLevel));

		BloodModObjFXRecipe r;
		r.bReplace = false;
		memset(r.bSet, 0, sizeof(r.bSet));
		memset(r.lo, 0, sizeof(r.lo));
		memset(r.hi, 0, sizeof(r.hi));
		memset(r.n, 0, sizeof(r.n));
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			const std::string before = a.Error();
			if (EqualNoCase(key, "Replace"))
			{
				r.bReplace = a.GetBool(tag, key, false);
				if (a.Error() != before) { sError = std::string(kPath) + ": " + a.Error(); return false; }
				continue;
			}
			int f = 0;
			while (f < BMO_FIELDS && !EqualNoCase(kFields[f].pName, key)) ++f;
			if (f == BMO_FIELDS)
			{
				snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\" (Replace, and the fields "
						 "Blood2/ModReference/ObjectFX.txt lists)", kPath, tag, key);
				sError = buf;
				return false;
			}
			const char* why = 0;
			std::string bad;
			switch (kFields[f].eKind)
			{
				case OK_NUMBER:
				case OK_SIGNED:
					if (a.IsString(tag, key))
					{
						bad = a.GetString(tag, key, "");
						if (!ParseRange(bad, r.lo[f][0], r.hi[f][0])) { why = "is not a number or a range a..b"; break; }
						bad.clear();
					}
					else
						r.lo[f][0] = r.hi[f][0] = a.GetFloat(tag, key, 0.0f);
					if (kFields[f].eKind == OK_NUMBER && (r.lo[f][0] < 0.0f || r.hi[f][0] < 0.0f)) why = "is below 0";
					break;
				case OK_VECTOR:
					if (a.IsString(tag, key))
					{
						bad = a.GetString(tag, key, "");
						if (!ParseVectorRange(bad, r.lo[f], r.hi[f])) { why = "is not a vector <x, y, z> of numbers or ranges"; break; }
						bad.clear();
					}
					else if (a.GetVector(tag, key, r.lo[f]))
						memcpy(r.hi[f], r.lo[f], sizeof(r.hi[f]));
					break;
				case OK_COUNT:
				{
					const int n = a.GetInt(tag, key, 0);
					if (n < 0) why = "is below 0";
					r.n[f] = (unsigned int)(n < 0 ? 0 : n);
					break;
				}
				case OK_EMIT:
				case OK_RAMP:
				case OK_FLAGS:
				{
					const std::string word = a.GetString(tag, key, "");
					if (a.Error() != before) break;
					if (kFields[f].eKind == OK_EMIT)
					{
						int i = 0;
						while (i < COUNT(kEmit) && !EqualNoCase(kEmit[i].pName, Trim(word).c_str())) ++i;
						if (i == COUNT(kEmit)) { why = "is not an emit type"; bad = word; }
						else r.n[f] = kEmit[i].nValue;
					}
					else if (!ParseWord(word, kFields[f].eKind == OK_RAMP ? kRamp : kFlags,
										kFields[f].eKind == OK_RAMP ? COUNT(kRamp) : COUNT(kFlags), r.n[f], bad))
						why = "names a flag there is not";
					break;
				}
				case OK_PATH:
					r.sParticle = a.GetString(tag, key, "");
					if (r.sParticle.size() > BloodMod::Table::kMaxString) why = "is longer than 99 characters";
					break;
			}
			if (a.Error() != before)
			{
				sError = std::string(kPath) + ": " + a.Error();
				return false;
			}
			if (why)
			{
				snprintf(buf, sizeof(buf), "%s: [%s]:%s %s%s%s%s", kPath, tag, key, why, bad.empty() ? "" : " (\"",
						 bad.c_str(), bad.empty() ? "" : "\")");
				sError = buf;
				return false;
			}
			r.bSet[f] = true;
		}
		for (int l = 0; l < 3; ++l)
			if (nLevel < 0 || nLevel == l)
				recipes[nId * 4 + l] = r;
	}
	g_Recipes.swap(recipes);
	return true;
}

std::string BloodModObjectFX_Summary()
{
	std::string out;
	for (std::map<int, BloodModObjFXRecipe>::const_iterator it = g_Recipes.begin(); it != g_Recipes.end(); ++it)
	{
		const char* pName = BloodMod_ObjFXName(it->first / 4);
		std::string line = std::string(pName ? pName : "?") + kLevels[it->first % 4] + ":";
		const BloodModObjFXRecipe& r = it->second;
		bool bAny = false;
		if (r.bReplace) { line += " Replace TRUE"; bAny = true; }
		for (int f = 0; f < BMO_FIELDS; ++f)
			if (r.bSet[f])
			{
				line += std::string(bAny ? ", " : " ") + kFields[f].pName + " " + BloodModObjectFX_Value(r, f);
				bAny = true;
			}
		if (!bAny) line += " nothing";
		out += std::string(out.empty() ? "" : "\n") + line;
	}
	return out;
}

std::string BloodModObjectFX_Reference()
{
	std::string out =
		"\n// The keys, the create struct's members without their type letter.  A key\n"
		"// replaces that field of retail's recipe; Replace = TRUE starts from an\n"
		"// empty one.  Retail's recipes are code (ClientShellDLL/ObjectFX.cpp):\n";
	static const char* const kKind[] = { "number, 0 or more", "number", "vector <x, y, z>", "whole number, 0 or more",
										 "an emit type", "ramp flags", "particle flags", "a sprite texture" };
	for (int f = 0; f < BMO_FIELDS; ++f)
		out += std::string("//     ") + kFields[f].pName + "  " + kKind[kFields[f].eKind] + "\n";
	out += "//\n// Emit types:";
	for (int i = 0; i < COUNT(kEmit); ++i) out += std::string(" ") + kEmit[i].pName;
	out += "\n// Ramp flags (RampUpType, RampDownType), joined by |:";
	for (int i = 0; i < COUNT(kRamp); ++i) out += std::string(" ") + kRamp[i].pName;
	out += "\n// Particle flags (FXFlags), joined by |:";
	for (int i = 0; i < COUNT(kFlags); ++i) out += std::string(" ") + kFlags[i].pName;
	out += "\n//\n// The effects, a section each, with _HIGH, _MEDIUM or _LOW for one detail\n"
		   "// level (ObjectParticles 2 or more, 1, 0):\n";
	for (int nId = 1; nId < 64; ++nId)
		if (const char* p = BloodMod_ObjFXName(nId))
			out += std::string("//     ") + p + "\n";
	return out;
}

void BloodModObjectFX_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModObjectFX_Load(src, sErr);
	if (!pPrint) return;
	BloodMod::PrintWrapped(bOk ? BloodModObjectFX_Summary() : sErr,
						   std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(kPath) + ": " : std::string()), pPrint);
}
