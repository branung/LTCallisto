// BloodModWeaponClasses.cpp: see BloodModWeaponClasses.h

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "BloodModWeaponClasses.h"
#include "BloodModWeapons.h"	// the WEAP_ names, a mod's new weapons too
#include "BloodModAI.h"			// the OBJFX_ names
#include "BloodModTable.h"		// PrintWrapped, DF

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"

#include <stdio.h>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{

const char* const kPath = "Mod\\WeaponClasses.txt";

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

enum Kind { WF_FLOAT, WF_BOOL, WF_DAMAGE, WF_AMMO, WF_STRING, WF_DAMAGE5, WF_OBJFX, WF_EXP, WF_PATH };
struct Key { const char* pName; Kind eKind; };
const Key kKeys[WK_COUNT] =
{
	{ "Chrome", WF_FLOAT }, { "MultiDamageBoost", WF_BOOL }, { "DamageType", WF_DAMAGE }, { "AltAmmoType", WF_AMMO },
	{ "PickupAmmo", WF_FLOAT }, { "PickupName", WF_STRING },
	{ "VoodooHits", WF_DAMAGE5 }, { "VoodooAltHit", WF_DAMAGE }, { "VoodooFX", WF_OBJFX },
	{ "HandExplosionType", WF_EXP }, { "HandExplosionSound", WF_PATH }, { "HandExplosionSoundRadius", WF_FLOAT },
	{ "MuzzleSmoke", WF_BOOL }, { "MuzzleLight", WF_BOOL }, { "Fragments", WF_BOOL },
	{ "HitFragments", WF_BOOL },
};
const size_t kMaxName = 63;

// The weapons with a pickup (WeaponPickups.cpp's m_nType).
const int kPickups[] =
{
	WEAP_BERETTA, WEAP_SHOTGUN, WEAP_SNIPERRIFLE, WEAP_ASSAULTRIFLE, WEAP_SUBMACHINEGUN, WEAP_FLAREGUN,
	WEAP_HOWITZER, WEAP_BUGSPRAY, WEAP_NAPALMCANNON, WEAP_MINIGUN, WEAP_VOODOO, WEAP_ORB, WEAP_DEATHRAY,
	WEAP_LIFELEECH, WEAP_TESLACANNON, WEAP_SINGULARITY,
#ifdef _ADD_ON
	WEAP_COMBATSHOTGUN, WEAP_FLAYER,
#endif
};

// The weapons whose class environment-maps its view model (m_nFlags FLAG_ENVIRONMENTMAP,
// GameWeapons.h and CDeathRay's constructor), the only ones CWeapon reads m_fChromeValue for
const int kChromed[] =
{
	WEAP_BERETTA, WEAP_SHOTGUN, WEAP_SNIPERRIFLE, WEAP_ASSAULTRIFLE, WEAP_SUBMACHINEGUN, WEAP_FLAREGUN,
	WEAP_HOWITZER, WEAP_BUGSPRAY, WEAP_NAPALMCANNON, WEAP_MINIGUN, WEAP_ORB, WEAP_TESLACANNON, WEAP_SINGULARITY,
	WEAP_MELEE, WEAP_DEATHRAY,
#ifdef _ADD_ON
	WEAP_COMBATSHOTGUN, WEAP_FLAYER,
#endif
};

// Retail's literals: CVoodooDoll::FireVector's hit types, its alt's, WonkyThemAll's effect, and
// each bomb's FireProjectile when it goes off in the hand (GameWeapons.cpp).
const int kRetailVoodooHits[5] = { DAMAGE_TYPE_NORMAL, DAMAGE_TYPE_NORMAL, DAMAGE_TYPE_BLIND, DAMAGE_TYPE_DROPWEAPON,
								   DAMAGE_TYPE_SLOW };
const int kRetailVoodooAlt = DAMAGE_TYPE_VISION;
const int kRetailVoodooFX = OBJFX_VOODOO_6;
const int kRetailHandType = EXP_DEFAULT_SMALL;
const char* const kRetailHandSound = "Sounds\\Weapons\\time\\explode.wav";
const float kRetailHandRadius = 1000.0f;

const char* const kDamage[] = { "NORMAL", "FIRE", "FREEZE", "SINGULARITY", "MELEE", "EXPLODE", "SUFFOCATE", "DEATH",
								"FREEFALL", "BLIND", "SLOW", "DROPWEAPON", "VISION", "ELECTRIC", "ACID", "LEECH" };
const int kNumDamage = (int)(sizeof(kDamage) / sizeof(kDamage[0]));
const char* const kAmmo[] = { "NONE", "BULLET", "SHELL", "BMG", "FLARE", "DIEBUGDIE", "HOWITZER", "FUEL", "BATTERY",
							  "FOCUS", "PROXIMITYBOMB", "REMOTEBOMB", "TIMEBOMB" };
const int kNumAmmo = (int)(sizeof(kAmmo) / sizeof(kAmmo[0]));
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

int FindIn(const char* const* pNames, int n, const char* p)
{
	for (int i = 0; i < n; ++i) if (EqualNoCase(pNames[i], p)) return i;
	return -1;
}

int FindName(Kind k, const char* p)
{
	if (k == WF_DAMAGE) return FindIn(kDamage, kNumDamage, p);
	if (k == WF_AMMO) { const int n = FindIn(kAmmo, kNumAmmo, p); return n > 0 ? n : -1; }
	if (k == WF_EXP)
	{
		for (int i = 0; i < kNumExp; ++i) if (EqualNoCase(kExp[i].pName, p)) return kExp[i].nId;
		return -1;
	}
	return BloodModAI_ObjFX(p);
}

const char* NameOf(Kind k, int n)
{
	if (k == WF_DAMAGE) return n >= 0 && n < kNumDamage ? kDamage[n] : "?";
	if (k == WF_AMMO) return n >= 0 && n < kNumAmmo ? kAmmo[n] : "?";
	if (k == WF_EXP)
	{
		for (int i = 0; i < kNumExp; ++i) if (kExp[i].nId == n) return kExp[i].pName;
		return "?";
	}
	const char* p = BloodModAI_ObjFXName(n);
	return p ? p : "?";
}

struct Value { bool bSet; float f; int n; int hits[5]; std::string s; };
std::map<int, std::vector<Value> > g_Mod;		// by WEAP_ id
std::set<int> g_Traced;
// A pickup keeps its name's pointer (PickupObject::m_szObjectName), so a mod's names are kept for
// the life of the module, as BloodModTable keeps its strings.
std::deque<std::string> g_KeptNames;

const Value* ModValue(int nWeapon, int nKey)
{
	std::map<int, std::vector<Value> >::const_iterator it = g_Mod.find(nWeapon);
	if (it == g_Mod.end() || nKey < 0 || nKey >= WK_COUNT) return 0;
	return it->second[(size_t)nKey].bSet ? &it->second[(size_t)nKey] : 0;
}

std::string ValueText(int nKey, const Value& v)
{
	const Kind k = kKeys[nKey].eKind;
	if (k == WF_FLOAT) return BloodMod::DF(v.f);
	if (k == WF_BOOL) return v.n ? "TRUE" : "FALSE";
	if (k == WF_STRING || k == WF_PATH) return "\"" + v.s + "\"";
	if (k == WF_DAMAGE5)
	{
		std::string s;
		for (int i = 0; i < 5; ++i) s += std::string(i ? " " : "") + NameOf(WF_DAMAGE, v.hits[i]);
		return "\"" + s + "\"";
	}
	return std::string("\"") + NameOf(k, v.n) + "\"";
}

bool Chromed(int nWeapon)
{
	for (size_t i = 0; i < sizeof(kChromed) / sizeof(kChromed[0]); ++i)
		if (kChromed[i] == nWeapon) return true;
	return false;
}

bool HasPickup(int nWeapon)
{
	for (size_t i = 0; i < sizeof(kPickups) / sizeof(kPickups[0]); ++i)
		if (kPickups[i] == nWeapon) return true;
	return false;
}

} // namespace

const char* BloodModWeaponClass_Key(int nKey)	{ return nKey >= 0 && nKey < WK_COUNT ? kKeys[nKey].pName : 0; }

bool BloodModWeaponClass_Has(int nWeapon, int nKey)
{
	if (nWeapon <= WEAP_NONE || !BloodModWeapon_Name(nWeapon) || nKey < 0 || nKey >= WK_COUNT) return false;
	const int nRetail = BloodModWeapon_Retail(nWeapon);
	switch (nKey)
	{
		case WK_Chrome:
			return Chromed(nRetail);		// a new weapon is its Base's class
		case WK_PickupAmmo: case WK_PickupName:
			return nRetail == nWeapon && HasPickup(nWeapon);	// a new weapon has no pickup class
		case WK_VoodooHits: case WK_VoodooAltHit: case WK_VoodooFX:
			return nWeapon == WEAP_VOODOO;
		case WK_HandExplosionType: case WK_HandExplosionSound: case WK_HandExplosionSoundRadius:
			return nWeapon == WEAP_PROXIMITYBOMB || nWeapon == WEAP_TIMEBOMB;	// the remote bomb's is commented out in retail
		case WK_HitFragments:
			return nRetail == WEAP_MELEE;		// UpdateWeaponFX tests the retail type
		default:
			return true;
	}
}

bool BloodModWeaponClass_Set(int nWeapon, int nKey)					{ return ModValue(nWeapon, nKey) != 0; }
float BloodModWeaponClass_ModFloat(int nWeapon, int nKey)			{ const Value* v = ModValue(nWeapon, nKey); return v ? v->f : 0.0f; }
int BloodModWeaponClass_ModInt(int nWeapon, int nKey)				{ const Value* v = ModValue(nWeapon, nKey); return v ? v->n : 0; }
const char* BloodModWeaponClass_ModString(int nWeapon, int nKey)
{
	const Value* v = ModValue(nWeapon, nKey);
	if (!v) return "";
	if (nKey != WK_PickupName) return v->s.c_str();
	for (std::deque<std::string>::const_iterator it = g_KeptNames.begin(); it != g_KeptNames.end(); ++it)
		if (*it == v->s) return it->c_str();
	g_KeptNames.push_back(v->s);
	return g_KeptNames.back().c_str();
}

int BloodModWeaponClass_VoodooHit(int nIndex)
{
	if (nIndex < 0 || nIndex > 4) nIndex = 0;
	const Value* v = ModValue(WEAP_VOODOO, WK_VoodooHits);
	return v ? v->hits[nIndex] : kRetailVoodooHits[nIndex];
}
int BloodModWeaponClass_VoodooAltHit()	{ const Value* v = ModValue(WEAP_VOODOO, WK_VoodooAltHit); return v ? v->n : kRetailVoodooAlt; }
int BloodModWeaponClass_VoodooFX()		{ const Value* v = ModValue(WEAP_VOODOO, WK_VoodooFX); return v ? v->n : kRetailVoodooFX; }
int BloodModWeaponClass_HandExplosionType(int nWeapon)
{
	const Value* v = ModValue(nWeapon, WK_HandExplosionType);
	return v ? v->n : kRetailHandType;
}
const char* BloodModWeaponClass_HandExplosionSound(int nWeapon)
{
	const Value* v = ModValue(nWeapon, WK_HandExplosionSound);
	return v ? v->s.c_str() : kRetailHandSound;
}
float BloodModWeaponClass_HandExplosionSoundRadius(int nWeapon)
{
	const Value* v = ModValue(nWeapon, WK_HandExplosionSoundRadius);
	return v ? v->f : kRetailHandRadius;
}

// UpdateWeaponFX's literals: no smoke and no wood or stone fragments for the minigun, a light for
// none, and a melee hit's sound and sparks without fragments. A new weapon is its Base's class.
bool BloodModWeaponClass_Flag(int nWeapon, int nKey)
{
	if (const Value* v = ModValue(nWeapon, nKey)) return v->n != 0;
	const int nRetail = BloodModWeapon_Retail(nWeapon);
	switch (nKey)
	{
		case WK_MuzzleSmoke: case WK_Fragments:	return nRetail != WEAP_MINIGUN;
		default:								return false;
	}
}

std::string BloodModWeaponClass_Format(int nKey, float f, int n, const char* s)
{
	if (nKey < 0 || nKey >= WK_COUNT) return "";
	Value v;
	v.bSet = true; v.f = f; v.n = n; v.s = s ? s : "";
	for (int i = 0; i < 5; ++i) v.hits[i] = 0;
	return ValueText(nKey, v);
}

std::string BloodModWeaponClass_Summary(int nWeapon)
{
	std::string line;
	for (int k = 0; k < WK_COUNT; ++k)
		if (const Value* v = ModValue(nWeapon, k))
			line += std::string(line.empty() ? "" : ", ") + kKeys[k].pName + " " + ValueText(k, *v);
	return line;
}

bool BloodModWeaponClass_FirstOf(int nWeapon, bool bPickup)	{ return g_Traced.insert(nWeapon * 2 + (bPickup ? 1 : 0)).second; }

void BloodModWeaponClasses_Clear()
{
	g_Mod.clear();
	g_Traced.clear();
}

bool BloodModWeaponClasses_Load(LT1Mod::FileSource &src, std::string &sError)
{
	BloodModWeaponClasses_Clear();
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

	std::map<int, std::vector<Value> > mod;
	char buf[512];
	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const int nWeapon = BloodModWeapon_Find(tag);
		if (nWeapon <= WEAP_NONE)
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is not a Blood 2 weapon (the WEAP_ names without the prefix, as in "
					 "Weapons.txt)", kPath, tag);
			sError = buf;
			return false;
		}
		std::vector<Value>& vals = mod[nWeapon];
		vals.resize((size_t)WK_COUNT);
		for (size_t i = 0; i < vals.size(); ++i) { vals[i].bSet = false; vals[i].f = 0.0f; vals[i].n = 0; }
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const char* key = keys[k].c_str();
			int nKey = -1;
			for (int i = 0; i < WK_COUNT && nKey < 0; ++i)
				if (EqualNoCase(kKeys[i].pName, key)) nKey = i;
			if (nKey < 0)
			{
				snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\" (Blood2/ModReference/WeaponClasses.txt lists them)",
						 kPath, tag, key);
				sError = buf;
				return false;
			}
			if (!BloodModWeaponClass_Has(nWeapon, nKey))
			{
				const char* pWho = nKey == WK_Chrome ? "only an environment-mapped weapon has it (its class sets FLAG_ENVIRONMENTMAP)" :
								   nKey == WK_PickupAmmo || nKey == WK_PickupName ? "only a retail weapon with a pickup has it" :
								   nKey == WK_HitFragments ? "only MELEE and a new weapon on it have it" :
								   nKey <= WK_VoodooFX ? "only VOODOO has it" : "only PROXIMITYBOMB and TIMEBOMB have it";
				snprintf(buf, sizeof(buf), "%s: [%s]:%s: %s", kPath, tag, key, pWho);
				sError = buf;
				return false;
			}
			Value& v = vals[(size_t)nKey];
			v.bSet = true;
			const std::string before = a.Error();
			bool bRange = true;
			const Kind kind = kKeys[nKey].eKind;
			if (kind == WF_FLOAT)
			{
				v.f = a.GetFloat(tag, key, 0.0f);
				bRange = v.f >= 0.0f && v.f <= 1e30f;
			}
			else if (kind == WF_BOOL)
				v.n = a.GetBool(tag, key, false) ? 1 : 0;
			else if (kind == WF_STRING || kind == WF_PATH)
			{
				v.s = a.GetString(tag, key, "");
				bRange = v.s.size() <= (kind == WF_STRING ? kMaxName : BloodMod::Table::kMaxString);
			}
			else if (kind == WF_DAMAGE5)
			{
				v.s = a.GetString(tag, key, "");
				std::vector<std::string> w;
				std::string cur;
				for (size_t i = 0; i <= v.s.size(); ++i)
				{
					if (i == v.s.size() || v.s[i] == ' ' || v.s[i] == '\t' || v.s[i] == ',')
					{
						if (!cur.empty()) w.push_back(cur);
						cur.clear();
					}
					else cur += v.s[i];
				}
				bool bNames = w.size() == 5;
				for (size_t i = 0; bNames && i < 5; ++i)
					bNames = (v.hits[i] = FindIn(kDamage, kNumDamage, w[i].c_str())) >= 0;
				if (a.Error() == before && !bNames)
				{
					snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not five DAMAGE_TYPE_ names without the prefix",
							 kPath, tag, key, v.s.c_str());
					sError = buf;
					return false;
				}
			}
			else
			{
				v.s = a.GetString(tag, key, "");
				v.n = FindName(kind, v.s.c_str());
				if (a.Error() == before && v.n < 0)
				{
					snprintf(buf, sizeof(buf), "%s: [%s]:%s: \"%s\" is not %s", kPath, tag, key, v.s.c_str(),
							 kind == WF_DAMAGE ? "a DAMAGE_TYPE_ name without the prefix" :
							 kind == WF_AMMO ? "an AMMO_ name without the prefix" :
							 kind == WF_EXP ? "an EXP_ name without the prefix" : "an OBJFX_ name without the prefix");
					sError = buf;
					return false;
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

std::string BloodModWeaponClasses_Summary()
{
	std::string out;
	for (std::map<int, std::vector<Value> >::const_iterator it = g_Mod.begin(); it != g_Mod.end(); ++it)
	{
		const std::string line = BloodModWeaponClass_Summary(it->first);
		if (!line.empty())
			out += std::string(out.empty() ? "" : "\n") + BloodModWeapon_Name(it->first) + ": " + line;
	}
	return out;
}

std::string BloodModWeaponClasses_Reference()
{
	std::string out;
	for (int w = WEAP_NONE + 1; w <= WEAP_MAXWEAPONTYPES; ++w)
	{
		if (!BloodModWeapon_Name(w)) continue;
		out += std::string("\n[") + BloodModWeapon_Name(w) + "]\n";
		for (int k = WK_MuzzleSmoke; k <= WK_HitFragments; ++k)
			if (BloodModWeaponClass_Has(w, k))
				out += std::string(kKeys[k].pName) + " = " + (BloodModWeaponClass_Flag(w, k) ? "TRUE" : "FALSE") + "\n";
		if (w == WEAP_VOODOO)
		{
			std::string hits;
			for (int i = 0; i < 5; ++i) hits += std::string(i ? " " : "") + kDamage[kRetailVoodooHits[i]];
			out += "VoodooHits = \"" + hits + "\"\nVoodooAltHit = \"" + kDamage[kRetailVoodooAlt] +
				   "\"\nVoodooFX = \"" + NameOf(WF_OBJFX, kRetailVoodooFX) + "\"\n";
		}
		if (BloodModWeaponClass_Has(w, WK_HandExplosionType))
			out += std::string("HandExplosionType = \"") + NameOf(WF_EXP, kRetailHandType) + "\"\nHandExplosionSound = \"" +
				   kRetailHandSound + "\"\nHandExplosionSoundRadius = " + BloodMod::DF(kRetailHandRadius) + "\n";
	}
	return out;
}

void BloodModWeaponClasses_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModWeaponClasses_Load(src, sErr);
	if (!pPrint) return;
	BloodMod::PrintWrapped(bOk ? BloodModWeaponClasses_Summary() : sErr,
						   std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(kPath) + ": " : std::string()), pPrint);
}
