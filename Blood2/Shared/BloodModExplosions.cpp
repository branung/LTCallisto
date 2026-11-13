// BloodModExplosions.cpp: see BloodModExplosions.h

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"			// the EXP_ ids
#include "BloodModExplosions.h"
#include "BloodModRules.h"		// the OBJFX_ names
#include "BloodModTable.h"		// PrintWrapped, DF, kMaxString

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

#ifdef _MSC_VER
#define strncasecmp _strnicmp	// MSVC spells it with an underscore
#endif

namespace
{

const char* const kPath = "Mod\\Explosions.txt";
bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

// SFXMsgIds.h's EXP_ values, by name without the prefix; the add-on's five
// only with _ADD_ON, as the header has them.
struct Exp { const char* pName; int nId; };
#define E(n) { #n, EXP_##n },
const Exp kExp[] =
{
	E(DEFAULT_SMALL) E(DEFAULT_MEDIUM) E(DEFAULT_LARGE) E(GRENADE) E(NAPALM_PRIMARY) E(NAPALM_ALT)
	E(NAPALM_FIREBALL) E(TESLA_PRIMARY) E(TESLA_ALT) E(HOWITZER_PRIMARY) E(HOWITZER_ALT) E(HOWITZER_MINI)
	E(FLAME_SMALL) E(FLARE_FIZZLE) E(FLARE_BURST) E(FLARE_FRAG) E(LIFELEECH_PRIMARY) E(LIFELEECH_ALT)
	E(SINGULARITY_PRIMARY) E(SHOCKBALL) E(SHOCKBALL_LARGE) E(ORB_BREAK) E(DIVINE_SHOCKBALL) E(RIFT_1)
	E(RIFT_2) E(DEATHRAY_PRIMARY) E(BUGSPRAY_PRIMARY) E(BUGSPRAY_ALT) E(NAGA_EYE_BEAM) E(NAGA_SPIKE)
	E(NAGA_STONE_CHUNK) E(NAGA_POUND_GROUND)
#ifdef _ADD_ON
	E(GAS_GRENADE) E(FLAYER_PRIMARY) E(FLAYER_ALT) E(FLAYER_RETRACT) E(FLAYER_SHATTER)
#endif
};
#undef E
const int kNumExp = (int)(sizeof(kExp) / sizeof(kExp[0]));

const char* const kKinds[BMX_KINDS] = { "Model", "Sprite", "Ring", "Light", "Frag", "Wave", "Flame" };

enum VKind { V_NUM, V_VEC, V_PATH, V_EXP, V_OBJFX };
#define KM (1 << BMX_MODEL)
#define KS (1 << BMX_SPRITE)
#define KR (1 << BMX_RING)
#define KL (1 << BMX_LIGHT)
#define KF (1 << BMX_FRAG)
#define KW (1 << BMX_WAVE)
#define KX (1 << BMX_FLAME)
#define KALL (KM | KS | KR | KL | KF | KW | KX)
struct FieldDef { const char* pName; VKind eKind; int nKinds; };
// In BloodModFxField's order.
const FieldDef kFields[BMF_FIELDS] =
{
	{ "Offset", V_NUM, KALL }, { "Normal", V_NUM, KALL & ~KL }, { "NormalVector", V_VEC, KALL & ~KL },
	{ "Count", V_NUM, KALL },
	{ "Scale1", V_VEC, KM | KS | KW }, { "Scale2", V_VEC, KM | KS | KW }, { "Rotations", V_VEC, KM },
	{ "Duration", V_NUM, KM | KS | KR | KL | KF | KW }, { "Alpha", V_NUM, KM | KS | KR | KW | KX },
	{ "WaveForm", V_NUM, KM | KS | KW }, { "FadeType", V_NUM, KM | KS | KR | KW | KX }, { "RandomRot", V_NUM, KM },
	{ "Model", V_PATH, KM | KF }, { "Skin", V_PATH, KM | KF }, { "Sprite", V_PATH, KS }, { "Align", V_NUM, KS | KR | KW },
	{ "Color", V_VEC, KR }, { "Radius", V_NUM, KR | KX }, { "PosRadius", V_NUM, KR | KX },
	{ "Velocity", V_NUM, KR | KF | KX }, { "Gravity", V_NUM, KR | KF | KX }, { "Rotation", V_NUM, KR },
	{ "Particles", V_NUM, KR | KX }, { "Delay", V_NUM, KR | KL | KW | KX }, { "RotateType", V_NUM, KR },
	{ "Particle", V_PATH, KR | KX }, { "Color1", V_VEC, KL | KX }, { "Color2", V_VEC, KL | KX }, { "Color3", V_VEC, KX },
	{ "LifeTimes", V_VEC, KX }, { "LifeOffsets", V_VEC, KX }, { "FXTimes", V_VEC, KX }, { "RampFlags", V_NUM, KX },
	{ "Radius1", V_NUM, KL }, { "Radius2", V_NUM, KL }, { "Scale", V_VEC, KF }, { "RotateMax", V_VEC, KF },
	{ "Spread", V_NUM, KF }, { "BounceMod", V_NUM, KF }, { "FadeTime", V_NUM, KF }, { "InitAlpha", V_NUM, KF },
	{ "RandDir", V_NUM, KF }, { "SpawnExp", V_NUM, KF }, { "SpawnType", V_EXP, KF }, { "TrailType", V_OBJFX, KF },
	{ "Wave", V_PATH, KW },
	{ "Pick", V_NUM, KM | KS | KR | KF | KW | KX },
};
const int kMaxPieces = 64;
const char* const kLevels[3] = { "_LOW", "_MEDIUM", "_HIGH" };

std::vector<std::string> g_NewNames;				// id BLOODMOD_EXP_FIRST_NEW + index
std::map<int, BloodModFxRecipe> g_Recipes;		// key id * 4 + level

// A number, or a..b.
bool ParseRange(const std::string& t, float& lo, float& hi)
{
	if (t.empty()) return false;
	const size_t dd = t.find("..", 1);
	char* e = 0;
	if (dd == std::string::npos)
	{
		lo = hi = (float)strtod(t.c_str(), &e);
		return e && !*e;
	}
	const std::string a = t.substr(0, dd), b = t.substr(dd + 2);
	lo = (float)strtod(a.c_str(), &e);
	if (!e || *e || a.empty()) return false;
	hi = (float)strtod(b.c_str(), &e);
	return e && !*e && !b.empty();
}

std::string RangeText(float lo, float hi)
{
	return lo == hi ? BloodMod::DF(lo) : BloodMod::DF(lo) + ".." + BloodMod::DF(hi);
}

// The tokens of a piece: whitespace apart, a <...> kept whole.
std::vector<std::string> Tokens(const char* p)
{
	std::vector<std::string> out;
	std::string cur;
	int depth = 0;
	for (; p && *p; ++p)
	{
		if (*p == '<') ++depth;
		if (*p == '>' && depth) --depth;
		if (!depth && (*p == ' ' || *p == '\t'))
		{
			if (!cur.empty()) out.push_back(cur);
			cur.clear();
		}
		else if (!(depth && (*p == ' ' || *p == '\t')))
			cur += *p;
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

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

bool ValidName(const std::string& s)
{
	if (s.empty() || s.size() > 31) return false;
	for (size_t i = 0; i < s.size(); ++i)
		if (!isalnum((unsigned char)s[i]) && s[i] != '_') return false;
	return true;
}

} // namespace

const char* BloodModExplosion_KindName(int nKind)	{ return nKind >= 0 && nKind < BMX_KINDS ? kKinds[nKind] : 0; }
const char* BloodModExplosion_FieldName(int nField)	{ return nField >= 0 && nField < BMF_FIELDS ? kFields[nField].pName : 0; }
int BloodModExplosion_NewCount()					{ return (int)g_NewNames.size(); }

int BloodModExplosion_Find(const char* pName)
{
	if (!pName || !pName[0]) return -1;
	for (int i = 0; i < kNumExp; ++i)
		if (EqualNoCase(kExp[i].pName, pName)) return kExp[i].nId;
	for (size_t i = 0; i < g_NewNames.size(); ++i)
		if (EqualNoCase(g_NewNames[i].c_str(), pName)) return BLOODMOD_EXP_FIRST_NEW + (int)i;
	return -1;
}

const char* BloodModExplosion_Name(int nId)
{
	for (int i = 0; i < kNumExp; ++i)
		if (kExp[i].nId == nId) return kExp[i].pName;
	const int n = nId - BLOODMOD_EXP_FIRST_NEW;
	return n >= 0 && n < (int)g_NewNames.size() ? g_NewNames[(size_t)n].c_str() : 0;
}

const BloodModFxRecipe* BloodModExplosion_Recipe(int nId, int nDetail)
{
	if (nDetail < 0) nDetail = 0;
	if (nDetail > 2) nDetail = 2;
	std::map<int, BloodModFxRecipe>::const_iterator it = g_Recipes.find(nId * 4 + nDetail);
	return it == g_Recipes.end() ? 0 : &it->second;
}

bool BloodModExplosion_ParsePiece(const char* pText, BloodModFxPiece& out, std::string& sError)
{
	out.nKind = -1;
	for (int f = 0; f < BMF_FIELDS; ++f)
	{
		BloodModFxValue& v = out.v[f];
		v.bSet = false; v.nId = 0; v.s.clear();
		for (int i = 0; i < 3; ++i) v.lo[i] = v.hi[i] = 0.0f;
	}
	const std::vector<std::string> t = Tokens(pText);
	if (t.empty()) { sError = "is empty (a kind, then Field=value pairs)"; return false; }
	for (int k = 0; k < BMX_KINDS; ++k)
		if (EqualNoCase(kKinds[k], t[0].c_str())) out.nKind = k;
	if (out.nKind < 0)
	{
		sError = "\"" + t[0] + "\" is no kind (Model, Sprite, Ring, Light, Frag, Wave, Flame)";
		return false;
	}
	for (size_t i = 1; i < t.size(); ++i)
	{
		const size_t eq = t[i].find('=');
		const std::string name = t[i].substr(0, eq), val = eq == std::string::npos ? "" : t[i].substr(eq + 1);
		int f = -1;
		for (int j = 0; j < BMF_FIELDS && f < 0; ++j)
			if (EqualNoCase(kFields[j].pName, name.c_str())) f = j;
		if (eq == std::string::npos || f < 0 || !(kFields[f].nKinds & (1 << out.nKind)))
		{
			sError = "\"" + t[i] + "\" is no " + kKinds[out.nKind] + " field" +
					 (eq == std::string::npos ? " (Field=value)" : "");
			return false;
		}
		BloodModFxValue& v = out.v[f];
		if (v.bSet) { sError = name + " is given twice"; return false; }
		v.bSet = true;
		bool bOk = true;
		switch (kFields[f].eKind)
		{
			case V_NUM:
				bOk = ParseRange(val, v.lo[0], v.hi[0]);
				if (bOk && f == BMF_COUNT) bOk = v.lo[0] >= 0 && v.hi[0] <= 255 && v.lo[0] == (int)v.lo[0] && v.hi[0] == (int)v.hi[0];
				if (bOk && f == BMF_PICK) bOk = v.lo[0] >= 0 && v.hi[0] <= 999 && v.lo[0] == (int)v.lo[0] && v.hi[0] == (int)v.hi[0];
				break;
			case V_VEC:
			{
				bOk = val.size() > 2 && val[0] == '<' && val[val.size() - 1] == '>';
				std::vector<std::string> parts;
				std::string cur;
				for (size_t c = 1; bOk && c + 1 < val.size(); ++c)
				{
					if (val[c] == ',') { parts.push_back(cur); cur.clear(); }
					else cur += val[c];
				}
				parts.push_back(cur);
				bOk = bOk && parts.size() == 3;
				for (int c = 0; bOk && c < 3; ++c) bOk = ParseRange(parts[(size_t)c], v.lo[c], v.hi[c]);
				break;
			}
			case V_PATH:
				v.s = val;
				// At most one %d, and no other %: the client formats it.
				bOk = !val.empty() && val.size() <= BloodMod::Table::kMaxString;
				for (size_t c = 0, n = 0; bOk && c < val.size(); ++c)
					if (val[c] == '%') bOk = c + 1 < val.size() && val[c + 1] == 'd' && ++n == 1;
				break;
			case V_EXP:
				v.s = val;
				v.nId = BloodModExplosion_Find(val.c_str());
				bOk = v.nId >= 0;
				if (bOk) v.s = BloodModExplosion_Name(v.nId);		// as the file names it, canonical
				break;
			case V_OBJFX:
				v.s = val;
				v.nId = BloodMod_ObjFX(val.c_str());
				bOk = v.nId >= 0;
				if (bOk) v.s = BloodMod_ObjFXName(v.nId);
				break;
		}
		if (!bOk)
		{
			const char* pWant = kFields[f].eKind == V_NUM ? (f == BMF_COUNT ? "a count, 0-255, or a..b" :
								f == BMF_PICK ? "a whole number, 0-999, or a..b" : "a number, or a..b") :
								kFields[f].eKind == V_VEC ? "a vector <x,y,z>, each a number or a..b" :
								kFields[f].eKind == V_PATH ? "a path (with at most one %d)" :
								kFields[f].eKind == V_EXP ? "an EXP_ name without the prefix" : "an OBJFX_ name without the prefix";
			sError = name + ": \"" + val + "\" is not " + pWant;
			return false;
		}
	}
	return true;
}

std::string BloodModExplosion_PieceText(const BloodModFxPiece& p)
{
	std::string out = BloodModExplosion_KindName(p.nKind) ? BloodModExplosion_KindName(p.nKind) : "?";
	for (int f = 0; f < BMF_FIELDS; ++f)
	{
		const BloodModFxValue& v = p.v[f];
		if (!v.bSet) continue;
		out += std::string(" ") + kFields[f].pName + "=";
		switch (kFields[f].eKind)
		{
			case V_NUM:	out += RangeText(v.lo[0], v.hi[0]); break;
			case V_VEC:	out += "<" + RangeText(v.lo[0], v.hi[0]) + "," + RangeText(v.lo[1], v.hi[1]) + "," +
							   RangeText(v.lo[2], v.hi[2]) + ">"; break;
			default:	out += v.s; break;
		}
	}
	return out;
}

void BloodModExplosions_Clear()
{
	g_NewNames.clear();
	g_Recipes.clear();
}

bool BloodModExplosions_Load(LT1Mod::FileSource &src, std::string &sError)
{
	BloodModExplosions_Clear();
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
	const std::vector<std::string>& tags = a.Tags();
	// New names first, in file order, so a piece's SpawnType can name one
	// further down.
	std::vector<std::string> names;
	for (size_t t = 0; t < tags.size(); ++t)
	{
		std::string name = tags[t];
		Level(name);
		bool bRetail = false;
		for (int i = 0; i < kNumExp && !bRetail; ++i) bRetail = EqualNoCase(kExp[i].pName, name.c_str());
		bool bSeen = false;
		for (size_t i = 0; i < names.size() && !bSeen; ++i) bSeen = EqualNoCase(names[i].c_str(), name.c_str());
		if (bRetail || bSeen) continue;
		if (!ValidName(name))
		{
			snprintf(buf, sizeof(buf), "%s: [%s]: a new explosion's name is letters, digits and _, at most 31", kPath,
					 tags[t].c_str());
			sError = buf;
			return false;
		}
		names.push_back(name);
	}
	if (names.size() > BLOODMOD_EXP_MAX_NEW)
	{
		snprintf(buf, sizeof(buf), "%s: %u new explosions, more than %d", kPath, (unsigned)names.size(), BLOODMOD_EXP_MAX_NEW);
		sError = buf;
		return false;
	}
	g_NewNames = names;

	std::map<int, BloodModFxRecipe> recipes;
	std::map<int, int> scope;		// id -> -1 all levels, or a bit per level a section names
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		std::string name = tags[t];
		const int nLevel = Level(name);
		const int nId = BloodModExplosion_Find(name.c_str());
		const bool bNew = nId >= BLOODMOD_EXP_FIRST_NEW;
		int& s = scope[nId];
		if ((nLevel < 0 && s) || (nLevel >= 0 && (s == -1 || (s & (1 << nLevel)))))
		{
			snprintf(buf, sizeof(buf), "%s: [%s]: %s is given twice for a detail level (a section for all three and "
					 "one for a level, or two)", kPath, tag, name.c_str());
			g_NewNames.clear();
			sError = buf;
			return false;
		}
		s = nLevel < 0 ? -1 : (s | (1 << nLevel));
		BloodModFxRecipe r;
		r.bReplace = false;
		std::map<int, BloodModFxPiece> pieces;
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			const std::string before = a.Error();
			if (EqualNoCase(key, "Replace"))
			{
				r.bReplace = a.GetBool(tag, key, false);
				if (a.Error() == before && bNew)
				{
					snprintf(buf, sizeof(buf), "%s: [%s]:Replace: %s is a new explosion, with no retail pieces to replace",
							 kPath, tag, name.c_str());
					g_NewNames.clear();
					sError = buf;
					return false;
				}
			}
			else if (!strncasecmp(key, "Piece", 5) && atoi(key + 5) >= 1 && atoi(key + 5) <= kMaxPieces)
			{
				const std::string text = a.GetString(tag, key, "");
				if (a.Error() == before)
				{
					std::string why;
					BloodModFxPiece p;
					if (!BloodModExplosion_ParsePiece(text.c_str(), p, why))
					{
						snprintf(buf, sizeof(buf), "%s: [%s]:%s: %s", kPath, tag, key, why.c_str());
						g_NewNames.clear();
						sError = buf;
						return false;
					}
					pieces[atoi(key + 5)] = p;
				}
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\" (Replace, and Piece1 .. Piece%d)", kPath, tag, key,
						 kMaxPieces);
				g_NewNames.clear();
				sError = buf;
				return false;
			}
			if (a.Error() != before)
			{
				sError = std::string(kPath) + ": " + a.Error();
				g_NewNames.clear();
				return false;
			}
		}
		for (std::map<int, BloodModFxPiece>::const_iterator it = pieces.begin(); it != pieces.end(); ++it)
			r.pieces.push_back(it->second);
		for (int l = 0; l < 3; ++l)
			if (nLevel < 0 || nLevel == l)
				recipes[nId * 4 + l] = r;
	}
	g_Recipes.swap(recipes);
	return true;
}

std::string BloodModExplosions_Summary()
{
	std::string out;
	for (std::map<int, BloodModFxRecipe>::const_iterator it = g_Recipes.begin(); it != g_Recipes.end(); ++it)
	{
		const int nId = it->first / 4, l = it->first % 4;
		const char* pName = BloodModExplosion_Name(nId);
		char line[200];
		snprintf(line, sizeof(line), "%s%s: %s%u piece%s%s", pName ? pName : "?", kLevels[l],
				 nId >= BLOODMOD_EXP_FIRST_NEW ? "new, " : "", (unsigned)it->second.pieces.size(),
				 it->second.pieces.size() == 1 ? "" : "s", it->second.bReplace ? ", replacing retail's" : "");
		out += std::string(out.empty() ? "" : "\n") + line;
	}
	return out;
}

std::string BloodModExplosions_Reference()
{
	std::string out = "\n// The kinds and their fields:\n";
	for (int k = 0; k < BMX_KINDS; ++k)
	{
		out += "//     " + std::string(kKinds[k]) + ":";
		for (int f = 0; f < BMF_FIELDS; ++f)
			if (kFields[f].nKinds & (1 << k)) out += std::string(" ") + kFields[f].pName;
		out += "\n";
	}
	out += "\n// Retail's explosions, which a section names (their pieces are code):\n";
	for (int i = 0; i < kNumExp; ++i)
		out += std::string("//     ") + kExp[i].pName + "\n";
	return out;
}

void BloodModExplosions_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModExplosions_Load(src, sErr);
	if (!pPrint) return;
	BloodMod::PrintWrapped(bOk ? BloodModExplosions_Summary() : sErr,
						   std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(kPath) + ": " : std::string()), pPrint);
}
