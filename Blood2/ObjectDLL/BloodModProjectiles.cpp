// BloodModProjectiles.cpp: see BloodModProjectiles.h

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "ProjectileConstants.h"
#include "BloodModProjectiles.h"
#include "BloodModExplosions.h"
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

enum Kind { PF_FLOAT, PF_INT, PF_BOOL, PF_DAMAGE, PF_OBJFX, PF_EXP, PF_PATH, PF_COLOR, PF_CLASS, PF_OBJFXLIST };

struct Key { const char* pName; Kind eKind; };
const Key kKeys[PK_COUNT] =
{
	{ "DamageType", PF_DAMAGE }, { "LifeTime", PF_FLOAT }, { "Gravity", PF_BOOL }, { "TrailFX", PF_OBJFX },
	{ "Explosion", PF_BOOL }, { "Model", PF_PATH }, { "Skin", PF_PATH },
	{ "Sprite", PF_BOOL }, { "Scale", PF_FLOAT }, { "AppearDelay", PF_FLOAT }, { "PointCollide", PF_BOOL },
	{ "NoSliding", PF_BOOL }, { "ClientFX", PF_BOOL },
	{ "BounceCount", PF_INT }, { "TimeDelay", PF_FLOAT },
	{ "SmokeTime", PF_FLOAT }, { "GasDamage", PF_FLOAT }, { "GasDamageRadius", PF_FLOAT },
	{ "GasDamageDelay", PF_FLOAT }, { "GasDamageType", PF_DAMAGE },
	{ "BounceDist", PF_FLOAT }, { "DetectRadius", PF_FLOAT }, { "DamageTime", PF_FLOAT },
	{ "AttractTime", PF_FLOAT }, { "SearchRange", PF_FLOAT }, { "ChainInterval", PF_FLOAT },
	{ "ExplosionType", PF_EXP }, { "ExplosionSound", PF_PATH }, { "ExplosionSoundRadius", PF_FLOAT },
	{ "FlySoundRadius", PF_FLOAT },
	{ "BurstAfterBounces", PF_BOOL }, { "SpinStops", PF_BOOL },
	{ "LightRadius", PF_FLOAT }, { "LightColor", PF_COLOR },
	{ "LoopSound", PF_PATH }, { "LoopSoundRadius", PF_FLOAT },
	{ "Children", PF_CLASS }, { "ChildCount", PF_INT }, { "ChildDamage", PF_FLOAT }, { "ChildRadius", PF_FLOAT },
	{ "ChildSpeed", PF_FLOAT }, { "ChildSideSpeedMin", PF_FLOAT }, { "ChildSideSpeedMax", PF_FLOAT },
	{ "ChildAngleMin", PF_FLOAT }, { "ChildAngleMax", PF_FLOAT },
	{ "BurstOnExpire", PF_BOOL }, { "BurstOnTouch", PF_BOOL }, { "BurstFX", PF_OBJFXLIST },
	{ "ExplosionAlongNormal", PF_BOOL }, { "AttachToHead", PF_BOOL },
};
const int kMaxChildren = 64;
const int kFirstClassKey = PK_BounceCount;		// below it, every class has the key
const int kFirstConstant = PK_SmokeTime;		// from it, retail's value is here

// The keys some classes have, and retail's value for a class constant (a
// member's is its constructor's, so none).
struct ClassKey { int nClass; int nKey; float f; int n; const char* s; float v[3]; };
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
	{ BMP_CGrenade, PK_BurstAfterBounces, 0, 0, 0 },
	{ BMP_CGrenade, PK_SpinStops, 0, 1, 0 },
	{ BMP_CSingularityProjectile, PK_ExplosionType, 0, SINGULARITY_EXPLOSION, 0 },
	{ BMP_CSingularityProjectile, PK_ExplosionSound, 0, 0, SINGULARITY_EXPLOSION_SOUND },
	{ BMP_CSingularityProjectile, PK_ExplosionSoundRadius, SINGULARITY_EXPLOSION_SOUND_RADIUS, 0, 0 },
	{ BMP_CModProjectile, PK_LightRadius, 0.0f, 0, 0 },
	{ BMP_CModProjectile, PK_LightColor, 0, 0, 0, { 1.0f, 1.0f, 1.0f } },
	{ BMP_CModProjectile, PK_LoopSound, 0, 0, "" },
	{ BMP_CModProjectile, PK_LoopSoundRadius, MODPROJ_LOOP_SOUND_RADIUS, 0, 0 },
	{ BMP_CModProjectile, PK_ExplosionType, 0, MODPROJ_EXPLOSION, 0 },
	{ BMP_CModProjectile, PK_ExplosionSound, 0, 0, MODPROJ_EXPLOSION_SOUND },
	{ BMP_CModProjectile, PK_ExplosionSoundRadius, MODPROJ_EXPLOSION_SOUND_RADIUS, 0, 0 },
	{ BMP_CModProjectile, PK_Children, 0, -1, "" },
	{ BMP_CModProjectile, PK_ChildCount, 0, 0, 0 },
	{ BMP_CModProjectile, PK_ChildDamage, 1.0f, 0, 0 },
	{ BMP_CModProjectile, PK_ChildRadius, 1.0f, 0, 0 },
	{ BMP_CModProjectile, PK_ChildSpeed, MODPROJ_CHILD_SPEED, 0, 0 },
	{ BMP_CModProjectile, PK_ChildSideSpeedMin, MODPROJ_CHILD_SIDE_MIN, 0, 0 },
	{ BMP_CModProjectile, PK_ChildSideSpeedMax, MODPROJ_CHILD_SIDE_MAX, 0, 0 },
	{ BMP_CModProjectile, PK_ChildAngleMin, MODPROJ_CHILD_ANGLE_MIN, 0, 0 },
	{ BMP_CModProjectile, PK_ChildAngleMax, MODPROJ_CHILD_ANGLE_MAX, 0, 0 },
	{ BMP_COrbProjectile, PK_AttachToHead, 0, 0, 0 },
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
	{ BMP_CGasGrenade, PK_BurstAfterBounces, 0, 0, 0 },
	{ BMP_CGasGrenade, PK_SpinStops, 0, 1, 0 },
	{ BMP_CGasGrenade, PK_LoopSound, 0, 0, GAS_GRENADE_LOOP_SOUND },
	{ BMP_CGasGrenade, PK_LoopSoundRadius, GAS_GRENADE_LOOP_SOUND_RADIUS, 0, 0 },
	{ BMP_CGasGrenade, PK_ExplosionType, 0, GAS_GRENADE_EXPLOSION, 0 },
	{ BMP_CGasGrenade, PK_ExplosionSound, 0, 0, GAS_GRENADE_EXPLOSION_SOUND },
	{ BMP_CGasGrenade, PK_ExplosionSoundRadius, GAS_GRENADE_EXPLOSION_SOUND_RADIUS, 0, 0 },
	{ BMP_CGasGrenade, PK_ExplosionAlongNormal, 0, 0, 0 },
	{ BMP_CGasGrenade, PK_BurstOnExpire, 0, 1, 0 },
	{ BMP_CGasGrenade, PK_BurstOnTouch, 0, 0, 0 },
	{ BMP_CGasGrenade, PK_BurstFX, 0, 0, "" },
#endif
};
const int kNumClassKeys = (int)(sizeof(kClassKeys) / sizeof(kClassKeys[0]));

// A mod's new classes: name and Base, an index into the whole list
struct NewClass { std::string name; int nBase; };
std::vector<NewClass> g_New;

int RootOf(int nClass)
{
	for (int i = 0; i <= BLOODMOD_PROJECTILE_MAX_NEW && nClass >= BMP_COUNT; ++i)
	{
		const size_t n = (size_t)(nClass - BMP_COUNT);
		nClass = n < g_New.size() ? g_New[n].nBase : -1;
	}
	return nClass >= 0 && nClass < BMP_COUNT ? nClass : -1;
}

const ClassKey* FindClassKey(int nClass, int nKey)
{
	nClass = RootOf(nClass);
	for (int i = 0; i < kNumClassKeys; ++i)
		if (kClassKeys[i].nClass == nClass && kClassKeys[i].nKey == nKey) return &kClassKeys[i];
	return 0;
}

// SharedDefs.h's DAMAGE_TYPE_ values, by name without the prefix.
const char* const kDamage[] = { "NORMAL", "FIRE", "FREEZE", "SINGULARITY", "MELEE", "EXPLODE", "SUFFOCATE", "DEATH",
								"FREEFALL", "BLIND", "SLOW", "DROPWEAPON", "VISION", "ELECTRIC", "ACID", "LEECH" };
const int kNumDamage = (int)(sizeof(kDamage) / sizeof(kDamage[0]));

// The EXP_ names, retail's and a mod's new ones (Mod\Explosions.txt): the shared list in
// Shared/BloodModExplosions.h

int FindName(const char* pName, int nKind)
{
	if (nKind == PF_DAMAGE)
	{
		for (int i = 0; i < kNumDamage; ++i) if (EqualNoCase(kDamage[i], pName)) return i;
		return -1;
	}
	if (nKind == PF_EXP)
	{
		return BloodModExplosion_Find(pName);
	}
	if (EqualNoCase(pName, "NONE")) return OBJFX_NONE;
	return BloodModAI_ObjFX(pName);
}

const char* NameOf(int nKind, int n)
{
	if (nKind == PF_DAMAGE) return n >= 0 && n < kNumDamage ? kDamage[n] : 0;
	if (nKind == PF_EXP)
	{
		return BloodModExplosion_Name(n);
	}
	return n == OBJFX_NONE ? "NONE" : BloodModAI_ObjFXName(n);
}

struct Value { bool bSet; float f; int n; std::string s; float v[3]; };
std::vector<std::vector<Value> > g_Mod;		// [class][key], empty with no mod
std::set<int> g_Traced;

// The class's own value, or the one its Base has from the mod.
const Value* ModValue(int nClass, int nKey)
{
	if (nKey < 0 || nKey >= PK_COUNT) return 0;
	for (int i = 0; i <= BLOODMOD_PROJECTILE_MAX_NEW && nClass >= 0 && (size_t)nClass < g_Mod.size(); ++i)
	{
		const Value& v = g_Mod[(size_t)nClass][(size_t)nKey];
		if (v.bSet) return &v;
		if (nClass < BMP_COUNT) return 0;
		const size_t n = (size_t)(nClass - BMP_COUNT);
		nClass = n < g_New.size() ? g_New[n].nBase : -1;
	}
	return 0;
}

std::string ValueText(int nKey, const Value& v)
{
	const Kind k = kKeys[nKey].eKind;
	if (k == PF_FLOAT) return BloodMod::DF(v.f);
	if (k == PF_INT) return std::to_string(v.n);
	if (k == PF_BOOL) return v.n ? "TRUE" : "FALSE";
	if (k == PF_PATH || k == PF_CLASS || k == PF_OBJFXLIST) return "\"" + v.s + "\"";
	if (k == PF_COLOR) return "<" + BloodMod::DF(v.v[0]) + ", " + BloodMod::DF(v.v[1]) + ", " + BloodMod::DF(v.v[2]) + ">";
	const char* p = NameOf(k, v.n);
	return std::string("\"") + (p ? p : "?") + "\"";
}

} // namespace

int BloodModProjectile_ClassCount()				{ return BMP_COUNT + (int)g_New.size(); }
int BloodModProjectile_NewCount()					{ return (int)g_New.size(); }
const char* BloodModProjectile_Class(int nClass)
{
	if (nClass >= 0 && nClass < BMP_COUNT) return kClasses[nClass];
	const size_t n = (size_t)(nClass - BMP_COUNT);
	return nClass >= BMP_COUNT && n < g_New.size() ? g_New[n].name.c_str() : 0;
}
int BloodModProjectile_Base(int nClass)
{
	const size_t n = (size_t)(nClass - BMP_COUNT);
	return nClass >= BMP_COUNT && n < g_New.size() ? g_New[n].nBase : -1;
}
int BloodModProjectile_Root(int nClass)				{ return RootOf(nClass); }
const char* BloodModProjectile_Key(int nKey)		{ return nKey >= 0 && nKey < PK_COUNT ? kKeys[nKey].pName : 0; }

int BloodModProjectile_FindClass(const char* pName)
{
	for (int i = 0; pName && i < BloodModProjectile_ClassCount(); ++i)
		if (EqualNoCase(BloodModProjectile_Class(i), pName)) return i;
	return -1;
}

bool BloodModProjectile_Has(int nClass, int nKey)
{
	if (RootOf(nClass) < 0 || nKey < 0 || nKey >= PK_COUNT) return false;
	return nKey < kFirstClassKey || FindClassKey(nClass, nKey) != 0;
}

bool BloodModProjectile_Set(int nClass, int nKey)				{ return ModValue(nClass, nKey) != 0; }
float BloodModProjectile_ModFloat(int nClass, int nKey)			{ const Value* v = ModValue(nClass, nKey); return v ? v->f : 0.0f; }
int BloodModProjectile_ModInt(int nClass, int nKey)				{ const Value* v = ModValue(nClass, nKey); return v ? v->n : 0; }
const char* BloodModProjectile_ModString(int nClass, int nKey)
{
	const int k = kKeys[nKey < 0 || nKey >= PK_COUNT ? 0 : nKey].eKind;
	const Value* v = k == PF_PATH || k == PF_CLASS || k == PF_OBJFXLIST ? ModValue(nClass, nKey) : 0;
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
	if (nKey < 0 || nKey >= PK_COUNT ||
		(kKeys[nKey].eKind != PF_PATH && kKeys[nKey].eKind != PF_CLASS && kKeys[nKey].eKind != PF_OBJFXLIST)) return "";
	if (const Value* v = ModValue(nClass, nKey)) return v->s.c_str();
	const ClassKey* c = FindClassKey(nClass, nKey);
	return c && c->s ? c->s : "";
}

int BloodModProjectile_FXList(int nClass, int nKey, int* pOut, int nMax)
{
	const std::string list = BloodModProjectile_String(nClass, nKey);
	int n = 0;
	size_t i = 0;
	while (i < list.size() && n < nMax)
	{
		const size_t j = list.find(' ', i);
		const std::string name = list.substr(i, j == std::string::npos ? std::string::npos : j - i);
		i = j == std::string::npos ? list.size() : j + 1;
		const int id = name.empty() ? -1 : BloodModAI_ObjFX(name.c_str());
		if (id >= 0) pOut[n++] = id;
	}
	return n;
}

void BloodModProjectile_Vector(int nClass, int nKey, float out[3])
{
	const Value* v = ModValue(nClass, nKey);
	const ClassKey* c = v ? 0 : FindClassKey(nClass, nKey);
	for (int i = 0; i < 3; ++i) out[i] = v ? v->v[i] : c ? c->v[i] : 0.0f;
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
	g_New.clear();
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

	// Built in scratch and committed only if the whole file is good.  The new
	// classes go in at once, so that the checks below see them; a bad file
	// puts the old ones back.
	char buf[512];
	const std::vector<std::string>& tags = a.Tags();
	std::vector<NewClass> added;
	std::vector<std::string> bases;
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const bool bNative = BloodModProjectile_FindClass(tag) >= 0 && BloodModProjectile_FindClass(tag) < BMP_COUNT;
		const std::string before = a.Error();
		const std::string sBase = a.GetString(tag, "Base", "");
		if (a.Error() != before)
		{
			sError = std::string(kPath) + ": " + a.Error();
			return false;
		}
		if (bNative && !sBase.empty())
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:Base: %s is a retail class, with code of its own; Base makes a new "
					 "class, under a name no class has", kPath, tag, tag);
			sError = buf;
			return false;
		}
		if (bNative)
			continue;
		if (sBase.empty())
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is not a Blood 2 projectile class (a weapon's ProjectileClass: "
					 "CGrenade, CTeslaProjectile, ...; Blood2/ModReference/Projectiles.txt lists them), and a new "
					 "class needs a Base", kPath, tag);
			sError = buf;
			return false;
		}
		for (size_t i = 0; i < added.size(); ++i)
			if (EqualNoCase(added[i].name.c_str(), tag))
			{
				snprintf(buf, sizeof(buf), "%s: [%s] is given twice", kPath, tag);
				sError = buf;
				return false;
			}
		if (added.size() >= (size_t)BLOODMOD_PROJECTILE_MAX_NEW)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]: at most %d new classes", kPath, tag, BLOODMOD_PROJECTILE_MAX_NEW);
			sError = buf;
			return false;
		}
		NewClass nc;
		nc.name = tag;
		nc.nBase = -1;
		added.push_back(nc);
		bases.push_back(sBase);
	}

	std::vector<NewClass> old;
	old.swap(g_New);
	g_New = added;
	struct Restore
	{
		std::vector<NewClass>& g; std::vector<NewClass>& o; bool bKeep;
		~Restore() { if (!bKeep) g.swap(o); }
	} restore = { g_New, old, false };

	for (size_t i = 0; i < g_New.size(); ++i)
	{
		g_New[i].nBase = BloodModProjectile_FindClass(bases[i].c_str());
		if (g_New[i].nBase < 0)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:Base: \"%s\" is not a projectile class, retail's or this file's",
					 kPath, g_New[i].name.c_str(), bases[i].c_str());
			sError = buf;
			return false;
		}
	}
	for (size_t i = 0; i < g_New.size(); ++i)
		if (RootOf(BMP_COUNT + (int)i) < 0)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:Base: its Bases come back to it", kPath, g_New[i].name.c_str());
			sError = buf;
			return false;
		}

	std::vector<std::vector<Value> > mod((size_t)BloodModProjectile_ClassCount(), std::vector<Value>((size_t)PK_COUNT));
	for (size_t c = 0; c < mod.size(); ++c)
		for (size_t k = 0; k < mod[c].size(); ++k)
		{
			Value& v = mod[c][k];
			v.bSet = false; v.f = 0.0f; v.n = 0; v.v[0] = v.v[1] = v.v[2] = 0.0f;
		}

	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const int nClass = BloodModProjectile_FindClass(tag);
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			if (EqualNoCase(key, "Base"))
				continue;
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
				snprintf(buf, sizeof(buf), "%s: [%s]:%s: only %s %s it%s", kPath, tag, key, who.c_str(),
						 who.find(',') == std::string::npos ? "has" : "have",
						 nClass >= BMP_COUNT ? ", and a new class has its Base's keys" : "");
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
					if (nKey == PK_ChildAngleMin || nKey == PK_ChildAngleMax) bRange = bRange && v.f <= 360.0f;
					break;
				case PF_INT:
					v.n = a.GetInt(tag, key, 0);
					v.f = (float)v.n;
					bRange = v.n >= 0 && (nKey != PK_ChildCount || v.n <= kMaxChildren);
					break;
				case PF_BOOL:
					v.n = a.GetBool(tag, key, false) ? 1 : 0;
					break;
				case PF_PATH:
				case PF_CLASS:
					v.s = a.GetString(tag, key, "");
					bRange = v.s.size() <= BloodMod::Table::kMaxString;
					if (kind == PF_CLASS && a.Error() == before && !v.s.empty() &&
						(v.n = BloodModProjectile_FindClass(v.s.c_str())) < 0)
					{
						snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not a projectile class, retail's or this "
								 "file's", kPath, tag, key, v.s.c_str());
						sError = buf;
						return false;
					}
					if (kind == PF_CLASS && v.s.empty()) v.n = -1;
					break;
				case PF_OBJFXLIST:
				{
					// OBJFX_ names, space-separated, each one the game has.
					v.s = a.GetString(tag, key, "");
					bRange = v.s.size() <= BloodMod::Table::kMaxString;
					size_t i = 0;
					while (a.Error() == before && i < v.s.size())
					{
						const size_t j = v.s.find(' ', i);
						const std::string name = v.s.substr(i, j == std::string::npos ? std::string::npos : j - i);
						i = j == std::string::npos ? v.s.size() : j + 1;
						if (!name.empty() && (EqualNoCase(name.c_str(), "NONE") || BloodModAI_ObjFX(name.c_str()) < 0))
						{
							snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not an OBJFX_ name without the prefix",
									 kPath, tag, key, name.c_str());
							sError = buf;
							return false;
						}
					}
					break;
				}
				case PF_COLOR:
					a.GetVector(tag, key, v.v);
					bRange = v.v[0] >= 0.0f && v.v[1] >= 0.0f && v.v[2] >= 0.0f &&
							 v.v[0] <= 1.0f && v.v[1] <= 1.0f && v.v[2] <= 1.0f;
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

	// Min below max, the class's own or its Base's, once the whole file is in.
	g_Mod.swap(mod);
	for (int c = 0; c < BloodModProjectile_ClassCount(); ++c)
	{
		const int pairs[2][2] = { { PK_ChildSideSpeedMin, PK_ChildSideSpeedMax }, { PK_ChildAngleMin, PK_ChildAngleMax } };
		for (int p = 0; p < 2; ++p)
			if (BloodModProjectile_Has(c, pairs[p][0]) &&
				BloodModProjectile_Float(c, pairs[p][0]) > BloodModProjectile_Float(c, pairs[p][1]))
			{
				snprintf(buf, sizeof(buf), "%s: [%s]: %s is above %s", kPath, BloodModProjectile_Class(c),
						 kKeys[pairs[p][0]].pName, kKeys[pairs[p][1]].pName);
				sError = buf;
				g_Mod.swap(mod);
				return false;
			}
	}
	restore.bKeep = true;
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
	for (int c = 0; c < BloodModProjectile_ClassCount() && !g_Mod.empty(); ++c)
	{
		// A class's own values; a new class's Base, which it starts from.
		std::string line;
		if (c >= BMP_COUNT)
			line = std::string("Base \"") + BloodModProjectile_Class(BloodModProjectile_Base(c)) + "\"";
		for (int k = 0; k < PK_COUNT; ++k)
		{
			const Value& v = g_Mod[(size_t)c][(size_t)k];
			if (v.bSet)
				line += std::string(line.empty() ? "" : ", ") + kKeys[k].pName + " " + ValueText(k, v);
		}
		if (!line.empty())
			out += std::string(out.empty() ? "" : "\n") + BloodModProjectile_Class(c) + ": " + line;
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
			for (int i = 0; i < 3; ++i) v.v[i] = ck.v[i];
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
