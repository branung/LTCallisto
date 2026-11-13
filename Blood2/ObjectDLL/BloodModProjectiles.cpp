// BloodModProjectiles.cpp: see BloodModProjectiles.h

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "ProjectileConstants.h"
#include "BloodModProjectiles.h"
#include "BloodModAI.h"			// the OBJFX_ names, which the AI's death table shares
#include "BloodModTable.h"		// PrintWrapped, DF

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"

#include <stdio.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>

namespace
{

const char* const kPath = "Mod\\Projectiles.txt";

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

const char* const kClasses[BMP_COUNT] =
{
#define X(c) #c,
	BLOODMOD_PROJECTILES_BASE(X)
	BLOODMOD_PROJECTILES_ADDON(X)
#undef X
};

enum Kind { PF_FLOAT, PF_INT, PF_BOOL, PF_DAMAGE, PF_OBJFX, PF_EXP, PF_PATH };

struct Key { const char* pName; Kind eKind; };
const Key kKeys[PK_COUNT] =
{
	{ "DamageType", PF_DAMAGE }, { "LifeTime", PF_FLOAT }, { "Gravity", PF_BOOL }, { "TrailFX", PF_OBJFX },
	{ "Explosion", PF_BOOL },
	{ "BounceCount", PF_INT }, { "TimeDelay", PF_FLOAT },
	{ "SmokeTime", PF_FLOAT }, { "GasDamage", PF_FLOAT }, { "GasDamageRadius", PF_FLOAT },
	{ "GasDamageDelay", PF_FLOAT }, { "GasDamageType", PF_DAMAGE },
	{ "BounceDist", PF_FLOAT }, { "DetectRadius", PF_FLOAT }, { "DamageTime", PF_FLOAT },
	{ "AttractTime", PF_FLOAT }, { "SearchRange", PF_FLOAT }, { "ChainInterval", PF_FLOAT },
	{ "ExplosionType", PF_EXP }, { "ExplosionSound", PF_PATH }, { "ExplosionSoundRadius", PF_FLOAT },
	{ "FlySoundRadius", PF_FLOAT },
};
const int kFirstClassKey = PK_BounceCount;		// below it, every class has the key
const int kFirstConstant = PK_SmokeTime;		// from it, retail's value is here

// The keys some classes have, and retail's value for a class constant (a
// member's is its constructor's, so none).
struct ClassKey { int nClass; int nKey; float f; int n; const char* s; };
const ClassKey kClassKeys[] =
{
	{ BMP_CGrenade, PK_BounceCount, 0, 0, 0 },
	{ BMP_CTimeBomb, PK_TimeDelay, 0, 0, 0 },
	{ BMP_CDeathRayProjectile, PK_BounceDist, DEATHRAY_BOUNCE_DIST, 0, 0 },
	{ BMP_COrbProjectile, PK_DetectRadius, THEORB_DETECT_RADIUS, 0, 0 },
	{ BMP_COrbProjectile, PK_DamageTime, THEORB_DAMAGE_TIME, 0, 0 },
	{ BMP_COrbAltProjectile, PK_DamageTime, THEORB_DAMAGE_TIME, 0, 0 },
	{ BMP_CSingularityProjectile, PK_AttractTime, SINGULARITY_ATTRACT_TIME, 0, 0 },
	{ BMP_CSingularityAltProjectile, PK_AttractTime, SINGULARITY_ATTRACT_TIME, 0, 0 },
	{ BMP_CTimeBomb, PK_ExplosionType, 0, TIMEBOMB_EXPLOSION, 0 },
	{ BMP_CTimeBomb, PK_ExplosionSound, 0, 0, TIMEBOMB_EXPLOSION_SOUND },
	{ BMP_CTimeBomb, PK_ExplosionSoundRadius, BOMB_EXPLOSION_SOUND_RADIUS, 0, 0 },
	{ BMP_CProximityBomb, PK_ExplosionType, 0, PROXIMITYBOMB_EXPLOSION, 0 },
	{ BMP_CProximityBomb, PK_ExplosionSound, 0, 0, PROXIMITYBOMB_EXPLOSION_SOUND },
	{ BMP_CProximityBomb, PK_ExplosionSoundRadius, BOMB_EXPLOSION_SOUND_RADIUS, 0, 0 },
	{ BMP_CRemoteBomb, PK_ExplosionType, 0, REMOTEBOMB_EXPLOSION, 0 },
	{ BMP_CRemoteBomb, PK_ExplosionSound, 0, 0, REMOTEBOMB_EXPLOSION_SOUND },
	{ BMP_CRemoteBomb, PK_ExplosionSoundRadius, BOMB_EXPLOSION_SOUND_RADIUS, 0, 0 },
	{ BMP_CSkullProjectile, PK_FlySoundRadius, SKULL_FLY_SOUND_RADIUS, 0, 0 },
#ifdef _ADD_ON
	{ BMP_CGasGrenade, PK_BounceCount, 0, 0, 0 },
	{ BMP_CRockProjectile, PK_BounceCount, 0, 0, 0 },
	{ BMP_CGasGrenade, PK_SmokeTime, GAS_GRENADE_SMOKE_TIME, 0, 0 },
	{ BMP_CGasGrenade, PK_GasDamage, GAS_GRENADE_MAX_DAMAGE, 0, 0 },
	{ BMP_CGasGrenade, PK_GasDamageRadius, GAS_GRENADE_DAMAGE_DIST, 0, 0 },
	{ BMP_CGasGrenade, PK_GasDamageDelay, GAS_GRENADE_DAMAGE_DELAY, 0, 0 },
	{ BMP_CGasGrenade, PK_GasDamageType, 0, DAMAGE_TYPE_NORMAL, 0 },
	{ BMP_CFlayerPortal, PK_SearchRange, FLAYER_ALT_SEARCH_RANGE, 0, 0 },
	{ BMP_CFlayerPortal, PK_ChainInterval, FLAYER_ALT_CHAIN_INTERVAL, 0, 0 },
#endif
};
const int kNumClassKeys = (int)(sizeof(kClassKeys) / sizeof(kClassKeys[0]));

const ClassKey* FindClassKey(int nClass, int nKey)
{
	for (int i = 0; i < kNumClassKeys; ++i)
		if (kClassKeys[i].nClass == nClass && kClassKeys[i].nKey == nKey) return &kClassKeys[i];
	return 0;
}

// SharedDefs.h's DAMAGE_TYPE_ values, by name without the prefix.
const char* const kDamage[] = { "NORMAL", "FIRE", "FREEZE", "SINGULARITY", "MELEE", "EXPLODE", "SUFFOCATE", "DEATH",
								"FREEFALL", "BLIND", "SLOW", "DROPWEAPON", "VISION", "ELECTRIC", "ACID", "LEECH" };
const int kNumDamage = (int)(sizeof(kDamage) / sizeof(kDamage[0]));

// SFXMsgIds.h's EXP_ values, by name without the prefix (ModScript.cpp's
// mod.explode takes the same).
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

int FindName(const char* pName, int nKind)
{
	if (nKind == PF_DAMAGE)
	{
		for (int i = 0; i < kNumDamage; ++i) if (EqualNoCase(kDamage[i], pName)) return i;
		return -1;
	}
	if (nKind == PF_EXP)
	{
		for (int i = 0; i < kNumExp; ++i) if (EqualNoCase(kExp[i].pName, pName)) return kExp[i].nId;
		return -1;
	}
	if (EqualNoCase(pName, "NONE")) return OBJFX_NONE;
	return BloodModAI_ObjFX(pName);
}

const char* NameOf(int nKind, int n)
{
	if (nKind == PF_DAMAGE) return n >= 0 && n < kNumDamage ? kDamage[n] : 0;
	if (nKind == PF_EXP)
	{
		for (int i = 0; i < kNumExp; ++i) if (kExp[i].nId == n) return kExp[i].pName;
		return 0;
	}
	return n == OBJFX_NONE ? "NONE" : BloodModAI_ObjFXName(n);
}

struct Value { bool bSet; float f; int n; std::string s; };
std::vector<std::vector<Value> > g_Mod;		// [class][key], empty with no mod
std::set<int> g_Traced;

const Value* ModValue(int nClass, int nKey)
{
	if (g_Mod.empty() || nClass < 0 || nClass >= BMP_COUNT || nKey < 0 || nKey >= PK_COUNT) return 0;
	const Value& v = g_Mod[(size_t)nClass][(size_t)nKey];
	return v.bSet ? &v : 0;
}

std::string ValueText(int nKey, const Value& v)
{
	const Kind k = kKeys[nKey].eKind;
	if (k == PF_FLOAT) return BloodMod::DF(v.f);
	if (k == PF_INT) return std::to_string(v.n);
	if (k == PF_BOOL) return v.n ? "TRUE" : "FALSE";
	if (k == PF_PATH) return "\"" + v.s + "\"";
	const char* p = NameOf(k, v.n);
	return std::string("\"") + (p ? p : "?") + "\"";
}

} // namespace

const char* BloodModProjectile_Class(int nClass)	{ return nClass >= 0 && nClass < BMP_COUNT ? kClasses[nClass] : 0; }
const char* BloodModProjectile_Key(int nKey)		{ return nKey >= 0 && nKey < PK_COUNT ? kKeys[nKey].pName : 0; }

int BloodModProjectile_FindClass(const char* pName)
{
	for (int i = 0; pName && i < BMP_COUNT; ++i)
		if (EqualNoCase(kClasses[i], pName)) return i;
	return -1;
}

bool BloodModProjectile_Has(int nClass, int nKey)
{
	if (nClass < 0 || nClass >= BMP_COUNT || nKey < 0 || nKey >= PK_COUNT) return false;
	return nKey < kFirstClassKey || FindClassKey(nClass, nKey) != 0;
}

bool BloodModProjectile_Set(int nClass, int nKey)				{ return ModValue(nClass, nKey) != 0; }
float BloodModProjectile_ModFloat(int nClass, int nKey)			{ const Value* v = ModValue(nClass, nKey); return v ? v->f : 0.0f; }
int BloodModProjectile_ModInt(int nClass, int nKey)				{ const Value* v = ModValue(nClass, nKey); return v ? v->n : 0; }
const char* BloodModProjectile_ModString(int nClass, int nKey)
{
	const Value* v = kKeys[nKey < 0 || nKey >= PK_COUNT ? 0 : nKey].eKind == PF_PATH ? ModValue(nClass, nKey) : 0;
	return v ? v->s.c_str() : "";
}

float BloodModProjectile_Float(int nClass, int nKey)
{
	if (const Value* v = ModValue(nClass, nKey)) return v->f;
	const ClassKey* c = FindClassKey(nClass, nKey);
	return c ? c->f : 0.0f;
}

int BloodModProjectile_Int(int nClass, int nKey)
{
	if (const Value* v = ModValue(nClass, nKey)) return v->n;
	const ClassKey* c = FindClassKey(nClass, nKey);
	return c ? c->n : 0;
}

const char* BloodModProjectile_String(int nClass, int nKey)
{
	if (nKey < 0 || nKey >= PK_COUNT || kKeys[nKey].eKind != PF_PATH) return "";
	if (const Value* v = ModValue(nClass, nKey)) return v->s.c_str();
	const ClassKey* c = FindClassKey(nClass, nKey);
	return c && c->s ? c->s : "";
}

int BloodModProjectile_Apply(int nClass, unsigned char& nDamageType, float& fLifeTime, int& nGravity,
							 unsigned int& dwTrailFX, unsigned int& bExplosion)
{
	int n = 0;
	if (const Value* v = ModValue(nClass, PK_DamageType)) { nDamageType = (unsigned char)v->n; ++n; }
	if (const Value* v = ModValue(nClass, PK_LifeTime)) { fLifeTime = v->f; ++n; }
	if (const Value* v = ModValue(nClass, PK_Gravity)) { nGravity = v->n ? 1 : 0; ++n; }
	if (const Value* v = ModValue(nClass, PK_TrailFX)) { dwTrailFX = (unsigned int)v->n; ++n; }
	if (const Value* v = ModValue(nClass, PK_Explosion)) { bExplosion = v->n ? 1 : 0; ++n; }
	return n;
}

std::string BloodModProjectile_Format(int nKey, float f, int n)
{
	if (nKey < 0 || nKey >= PK_COUNT) return "";
	Value v;
	v.bSet = true; v.f = f; v.n = n;
	return ValueText(nKey, v);
}

bool BloodModProjectile_FirstOfClass(int nClass)	{ return g_Traced.insert(nClass).second; }

void BloodModProjectiles_Clear()
{
	g_Mod.clear();
	g_Traced.clear();
}

bool BloodModProjectiles_Load(LT1Mod::FileSource &src, std::string &sError)
{
	BloodModProjectiles_Clear();
	sError.clear();
	if (!LT1Mod::IsActive())
		return true;

	std::string text;
	if (!src.ReadFile(kPath, text))
		return true;							// a mod need not touch the projectiles

	LT1Mod::Attributes a;
	if (!a.Parse(text.data(), text.size(), kPath))
	{
		sError = a.Error();
		return false;
	}

	// Built in scratch and committed only if the whole file is good.
	std::vector<std::vector<Value> > mod((size_t)BMP_COUNT, std::vector<Value>((size_t)PK_COUNT));
	for (size_t c = 0; c < mod.size(); ++c)
		for (size_t k = 0; k < mod[c].size(); ++k) { mod[c][k].bSet = false; mod[c][k].f = 0.0f; mod[c][k].n = 0; }
	char buf[512];

	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const int nClass = BloodModProjectile_FindClass(tag);
		if (nClass < 0)
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is not a Blood 2 projectile class (a weapon's ProjectileClass: "
					 "CGrenade, CTeslaProjectile, ...; Blood2/ModReference/Projectiles.txt lists them)", kPath, tag);
			sError = buf;
			return false;
		}
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			int nKey = -1;
			for (int i = 0; i < PK_COUNT && nKey < 0; ++i)
				if (EqualNoCase(kKeys[i].pName, key)) nKey = i;
			if (nKey < 0)
			{
				snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\" (Blood2/ModReference/Projectiles.txt lists them)",
						 kPath, tag, key);
				sError = buf;
				return false;
			}
			if (!BloodModProjectile_Has(nClass, nKey))
			{
				std::string who;
				for (int i = 0; i < kNumClassKeys; ++i)
					if (kClassKeys[i].nKey == nKey)
						who += std::string(who.empty() ? "" : ", ") + kClasses[kClassKeys[i].nClass];
				snprintf(buf, sizeof(buf), "%s: [%s]:%s: only %s %s it", kPath, tag, key, who.c_str(),
						 who.find(',') == std::string::npos ? "has" : "have");
				sError = buf;
				return false;
			}
			Value& v = mod[(size_t)nClass][(size_t)nKey];
			v.bSet = true;
			const std::string before = a.Error();
			bool bRange = true;
			const Kind kind = kKeys[nKey].eKind;
			switch (kind)
			{
				case PF_FLOAT:
					v.f = a.GetFloat(tag, key, 0.0f);
					bRange = v.f >= 0.0f && v.f <= 1e30f;
					break;
				case PF_INT:
					v.n = a.GetInt(tag, key, 0);
					v.f = (float)v.n;
					bRange = v.n >= 0;
					break;
				case PF_BOOL:
					v.n = a.GetBool(tag, key, false) ? 1 : 0;
					break;
				case PF_PATH:
					v.s = a.GetString(tag, key, "");
					bRange = v.s.size() <= BloodMod::Table::kMaxString;
					break;
				default:
				{
					v.s = a.GetString(tag, key, "");
					v.n = FindName(v.s.c_str(), kind);
					if (a.Error() == before && v.n < 0)
					{
						snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not %s", kPath, tag, key, v.s.c_str(),
								 kind == PF_DAMAGE ? "a DAMAGE_TYPE_ name without the prefix (FIRE, ACID, ...)" :
								 kind == PF_EXP ? "an EXP_ name without the prefix (DEFAULT_SMALL, ...)" :
												  "an OBJFX_ name without the prefix, or NONE");
						sError = buf;
						return false;
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
				snprintf(buf, sizeof(buf), "%s: [%s]:%s is out of range", kPath, tag, key);
				sError = buf;
				return false;
			}
		}
	}
	g_Mod.swap(mod);
	return true;
}

std::string BloodModProjectile_ClassSummary(int nClass)
{
	std::string line;
	for (int k = 0; k < PK_COUNT; ++k)
		if (const Value* v = ModValue(nClass, k))
			line += std::string(line.empty() ? "" : ", ") + kKeys[k].pName + " " + ValueText(k, *v);
	return line;
}

std::string BloodModProjectiles_Summary()
{
	std::string out;
	for (int c = 0; c < BMP_COUNT && !g_Mod.empty(); ++c)
	{
		std::string line;
		for (int k = 0; k < PK_COUNT; ++k)
			if (const Value* v = ModValue(c, k))
				line += std::string(line.empty() ? "" : ", ") + kKeys[k].pName + " " + ValueText(k, *v);
		if (!line.empty())
			out += std::string(out.empty() ? "" : "\n") + kClasses[c] + ": " + line;
	}
	return out;
}

std::string BloodModProjectiles_Reference()
{
	std::string out;
	for (int c = 0; c < BMP_COUNT; ++c)
	{
		out += std::string("\n[") + kClasses[c] + "]\n";
		std::string members;
		for (int i = 0; i < kNumClassKeys; ++i)
		{
			const ClassKey& ck = kClassKeys[i];
			if (ck.nClass != c) continue;
			if (ck.nKey < kFirstConstant)
			{
				members += std::string(" ") + kKeys[ck.nKey].pName;
				continue;
			}
			Value v;
			v.bSet = true; v.f = ck.f; v.n = ck.n; v.s = ck.s ? ck.s : "";
			out += std::string(kKeys[ck.nKey].pName) + " = " + ValueText(ck.nKey, v) + "\n";
		}
		if (!members.empty())
			out += "// and, the constructor's:" + members + "\n";
	}
	return out;
}

void BloodModProjectiles_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModProjectiles_Load(src, sErr);
	if (!pPrint) return;
	BloodMod::PrintWrapped(bOk ? BloodModProjectiles_Summary() : sErr,
						   std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(kPath) + ": " : std::string()), pPrint);
}
