// weaponclasses.cpp: testBlood2Tables for Mod\WeaponClasses.txt (BloodModWeaponClasses.h).
//
//   literals      with no mod, the voodoo doll's and the bombs' values are
//                 GameWeapons.cpp's, transcribed here.
//   keys          every weapon takes three every-weapon keys, and Chrome the
//                 environment-mapped ones; the pickup
//                 keys load on the weapons with a pickup, the voodoo keys on
//                 VOODOO and the in-hand bursts on PROXIMITYBOMB and
//                 TIMEBOMB, and each is refused by name on every other.
//   round trip    the --dump file loads as a mod and changes no literal.
//   errors        a bad file loads nothing, over a mod loaded before it.

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "BloodModWeapons.h"
#include "BloodModWeaponClasses.h"
#include "harness.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{

bool LoadClasses(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\WeaponClasses.txt", text, err, BloodModWeaponClasses_Load);
}

void Unload()
{
	BloodModWeaponClasses_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// The literals' values, as one string, to compare against retail's.
std::string Literals()
{
	char b[512];
	snprintf(b, sizeof(b), "%d %d %d %d %d / %d / %d / %d %s %g / %d %s %g", BloodModWeaponClass_VoodooHit(0),
			 BloodModWeaponClass_VoodooHit(1), BloodModWeaponClass_VoodooHit(2), BloodModWeaponClass_VoodooHit(3),
			 BloodModWeaponClass_VoodooHit(4), BloodModWeaponClass_VoodooAltHit(), BloodModWeaponClass_VoodooFX(),
			 BloodModWeaponClass_HandExplosionType(WEAP_PROXIMITYBOMB), BloodModWeaponClass_HandExplosionSound(WEAP_PROXIMITYBOMB),
			 BloodModWeaponClass_HandExplosionSoundRadius(WEAP_PROXIMITYBOMB),
			 BloodModWeaponClass_HandExplosionType(WEAP_TIMEBOMB), BloodModWeaponClass_HandExplosionSound(WEAP_TIMEBOMB),
			 BloodModWeaponClass_HandExplosionSoundRadius(WEAP_TIMEBOMB));
	return b;
}
std::string g_Retail;

// UpdateWeaponFX's flags for every weapon, as one string
std::string Flags()
{
	std::string s;
	for (int w = 1; w <= WEAP_MAXWEAPONTYPES; ++w)
		for (int k = WK_MuzzleSmoke; k <= WK_HitFragments; ++k)
			s += BloodModWeaponClass_Has(w, k) ? (BloodModWeaponClass_Flag(w, k) ? '1' : '0') : '-';
	return s;
}

void TestLiterals()
{
	printf("weapon classes: literals\n");
	Unload();
	// GameWeapons.cpp as retail's source had it: CVoodooDoll::FireVector's cases and alt,
	// WonkyThemAll's effect, and the two bombs' in-hand bursts.
	char want[512];
	snprintf(want, sizeof(want), "%d %d %d %d %d / %d / %d / %d %s %g / %d %s %g", DAMAGE_TYPE_NORMAL, DAMAGE_TYPE_NORMAL,
			 DAMAGE_TYPE_BLIND, DAMAGE_TYPE_DROPWEAPON, DAMAGE_TYPE_SLOW, DAMAGE_TYPE_VISION, OBJFX_VOODOO_6,
			 EXP_DEFAULT_SMALL, "Sounds\\Weapons\\time\\explode.wav", 1000.0, EXP_DEFAULT_SMALL,
			 "Sounds\\Weapons\\time\\explode.wav", 1000.0);
	Check(g_Retail == want, "with no mod the voodoo doll's hits, alt and effect and the bombs' in-hand bursts are "
							"retail's: " + g_Retail);
	// UpdateWeaponFX as retail's source had it: `m_nType != WEAP_MINIGUN` for the muzzle smoke and
	// the wood and stone fragments, no muzzle light, and a melee hit's WFX_SOUND | WFX_SPARKS alone.
	bool bFlags = true;
	for (int w = 1; w <= WEAP_MAXWEAPONTYPES; ++w)
	{
		const bool bMini = w == WEAP_MINIGUN;
		bFlags = bFlags && BloodModWeaponClass_Flag(w, WK_MuzzleSmoke) == !bMini &&
				 BloodModWeaponClass_Flag(w, WK_Fragments) == !bMini && !BloodModWeaponClass_Flag(w, WK_MuzzleLight) &&
				 BloodModWeaponClass_Has(w, WK_HitFragments) == (w == WEAP_MELEE) &&
				 !BloodModWeaponClass_Flag(w, WK_HitFragments);
	}
	Check(bFlags, "with no mod every weapon but the minigun smokes and throws fragments, none lights its muzzle, and "
				  "only MELEE has HitFragments, FALSE");
	// CDeathRay::Fire's alt sweep and CFlareGun::FireSpread's eight-point pattern before the keys:
	// each its own class's, and on.
	bool bClass = true;
	for (int w = 1; w <= WEAP_MAXWEAPONTYPES; ++w)
		bClass = bClass && BloodModWeaponClass_Has(w, WK_AltSweep) == (w == WEAP_DEATHRAY) &&
				 BloodModWeaponClass_Has(w, WK_SpreadPattern) == (w == WEAP_FLAREGUN) &&
				 BloodModWeaponClass_Flag(w, WK_AltSweep) == (w == WEAP_DEATHRAY) &&
				 BloodModWeaponClass_Flag(w, WK_SpreadPattern) == (w == WEAP_FLAREGUN);
	Check(bClass, "with no mod only DEATHRAY has AltSweep and only FLAREGUN SpreadPattern, each TRUE");
	std::string err;
	Check(LoadClasses("[DEATHRAY]\nAltSweep = FALSE\n\n[FLAREGUN]\nSpreadPattern = FALSE\n", err) &&
		  !BloodModWeaponClass_Flag(WEAP_DEATHRAY, WK_AltSweep) && !BloodModWeaponClass_Flag(WEAP_FLAREGUN, WK_SpreadPattern),
		  "and a mod turns each off");
	Unload();
}

void TestKeys()
{
	printf("weapon classes: keys\n");
	std::string err;
	int nCases = 0, nBad = 0, nRefused = 0;
	for (int w = 1; w <= WEAP_MAXWEAPONTYPES; ++w)
		for (int k = 0; k < WK_COUNT; ++k)
		{
			const char* key = BloodModWeaponClass_Key(k);
			std::string v = "2.0";
			if (k == WK_Tracer) v = "\"YELLOW_SMALL\"";
			else if (k == WK_MultiDamageBoost || k >= WK_MuzzleSmoke) v = "TRUE";
			else if (k == WK_DamageType || k == WK_VoodooAltHit) v = "\"ACID\"";
			else if (k == WK_AltAmmoType) v = "\"FUEL\"";
			else if (k == WK_PickupName) v = "\"Mod Gun\"";
			else if (k == WK_VoodooHits) v = "\"FIRE SINGULARITY ELECTRIC NORMAL ACID\"";
			else if (k == WK_VoodooFX) v = "\"VOODOO_1\"";
			else if (k == WK_HandExplosionType) v = "\"NAPALM_PRIMARY\"";
			else if (k == WK_HandExplosionSound) v = "\"Sounds\\Weapons\\napalm\\altimpact.wav\"";
			const std::string text = std::string("[") + BloodModWeapon_Name(w) + "]\n" + key + " = " + v + "\n";
			++nCases;
			err.clear();
			const bool bLoaded = LoadClasses(text, err);
			bool bOk;
			if (BloodModWeaponClass_Has(w, k))
			{
				bOk = bLoaded && BloodModWeaponClass_Set(w, k) &&
					  BloodModWeaponClass_Summary(w) == std::string(key) + " " + v;
				if (k == WK_VoodooHits)
					bOk = bOk && BloodModWeaponClass_VoodooHit(1) == DAMAGE_TYPE_SINGULARITY;
				if (k == WK_HandExplosionType)
					bOk = bOk && BloodModWeaponClass_HandExplosionType(w) == EXP_NAPALM_PRIMARY;
				if (k == WK_AltAmmoType)
					bOk = bOk && BloodModWeaponClass_ModInt(w, k) == AMMO_FUEL;
				if (k == WK_PickupName)
					bOk = bOk && !strcmp(BloodModWeaponClass_ModString(w, k), "Mod Gun");
				if (k == WK_Tracer)
					bOk = bOk && BloodModWeaponClass_Tracer(w) == LASER_YELLOW_SMALL;
			}
			else
			{
				bOk = !bLoaded && err.find(std::string("]:") + key + ": only ") != std::string::npos;
				nRefused += bOk;
			}
			if (!bOk && ++nBad <= 6)
				printf("        [%s] %s = %s: loaded %d, \"%s\" %s\n", BloodModWeapon_Name(w), key, v.c_str(), (int)bLoaded,
					   BloodModWeaponClass_Summary(w).c_str(), err.c_str());
		}
	char b[200];
	snprintf(b, sizeof(b), "%d single-key files over every weapon: each loads where the weapon has the key, and the "
			 "%d elsewhere are refused naming who has it", nCases, nRefused);
	Check(nBad == 0, b);
	Check(BloodModWeaponClass_Has(WEAP_SHOTGUN, WK_PickupAmmo) && !BloodModWeaponClass_Has(WEAP_MELEE, WK_PickupAmmo) &&
		  !BloodModWeaponClass_Has(WEAP_REMOTEBOMB, WK_HandExplosionType) &&
		  BloodModWeaponClass_Has(WEAP_TIMEBOMB, WK_HandExplosionType) &&
		  BloodModWeaponClass_Has(WEAP_MELEE, WK_Chrome) && !BloodModWeaponClass_Has(WEAP_VOODOO, WK_Chrome) &&
		  !BloodModWeaponClass_Has(WEAP_SHIKARI_CLAW, WK_Chrome),
		  "the knife has no pickup, the remote bomb no in-hand burst (retail's is commented out), and only an "
		  "environment-mapped weapon a Chrome (the knife yes, the voodoo doll and a Shikari's claw no)");
	Unload();
}

void TestRoundTrip()
{
	printf("weapon classes: round trip\n");
	std::string err;
	Unload();
	const std::string dump = WeaponClassesDump();
	const std::string flags = Flags();
	Check(LoadClasses(dump, err) && Literals() == g_Retail && Flags() == flags,
		  "the dump loads as a mod and leaves every literal retail's" + (err.empty() ? std::string() : ": " + err));
	std::string bent = dump;
	const size_t at = bent.find("VoodooFX = \"VOODOO_6\"");
	if (at != std::string::npos) bent.replace(at, strlen("VoodooFX = \"VOODOO_6\""), "VoodooFX = \"VOODOO_1\"");
	Check(LoadClasses(bent, err) && BloodModWeaponClass_VoodooFX() == OBJFX_VOODOO_1 && Literals() != g_Retail,
		  "calibration: a dump with VoodooFX moved is caught");
	Unload();
}

void TestErrors()
{
	printf("weapon classes: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a weapon there is not",			"[LASER]\nChrome = 0.1\n",							"[LASER] is not a Blood 2 weapon" },
		{ "a key there is not",			"[SHOTGUN]\nSpeed = 0.1\n",							"has no key \"Speed\"" },
		{ "a pickup on the knife",			"[MELEE]\nPickupAmmo = 5.0\n",						"[MELEE]:PickupAmmo: only a retail weapon with a pickup" },
		{ "voodoo keys on the shotgun",	"[SHOTGUN]\nVoodooFX = \"VOODOO_1\"\n",				"[SHOTGUN]:VoodooFX: only VOODOO" },
		{ "four voodoo hits",				"[VOODOO]\nVoodooHits = \"FIRE ACID NORMAL SLOW\"\n",	"is not five DAMAGE_TYPE_ names" },
		{ "an ammo there is not",			"[SHOTGUN]\nAltAmmoType = \"ROCKET\"\n",				"\"ROCKET\" is not an AMMO_ name" },
		{ "NONE as the alt ammo",			"[SHOTGUN]\nAltAmmoType = \"NONE\"\n",				"\"NONE\" is not an AMMO_ name" },
		{ "a negative chrome",				"[SHOTGUN]\nChrome = -1.0\n",						"[SHOTGUN]:Chrome is out of range" },
		{ "a 64-character pickup name",	0,													"[SHOTGUN]:PickupName is out of range" },
	};
	const std::string sLong = "[SHOTGUN]\nPickupName = \"" + std::string(64, 'x') + "\"\n";
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadClasses("[VOODOO]\nVoodooFX = \"VOODOO_1\"\n", err);
		const bool bBefore = BloodModWeaponClass_VoodooFX() == OBJFX_VOODOO_1;
		err.clear();
		const bool bLoaded = LoadClasses(bad[i].text ? bad[i].text : sLong, err);
		Check(bBefore && !bLoaded && Literals() == g_Retail && err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	Unload();
}

} // namespace

std::string WeaponClassesDump()
{
	return
		"// Blood II's weapon classes and the constants a mod's\n"
		"// Mod\\WeaponClasses.txt can set for them. GENERATED by `testBlood2Tables\n"
		"// --dump weaponclasses` from Blood2/ObjectDLL/BloodModWeaponClasses.cpp; do\n"
		"// not edit, regenerate.\n"
		"//\n"
		"// A section is a weapon's WEAP_ name, as in Weapons.txt.  Every weapon\n"
		"// takes MultiDamageBoost, DamageType (a DAMAGE_TYPE_ name) and AltAmmoType\n"
		"// (an AMMO_ name), and an environment-mapped one Chrome, over its class's\n"
		"// constructor's values, and\n"
		"// a weapon with a pickup PickupAmmo and PickupName over its pickup's;\n"
		"// those are not listed here.  With Mod\\AI.txt's [Options] Trace = TRUE\n"
		"// the server prints them.  Below are the literals in the code that a\n"
		"// mod can change, with retail's values: every weapon's MuzzleSmoke,\n"
		"// MuzzleLight and Fragments, for its hits with BULLET, SHELL or BMG\n"
		"// ammo, and a melee weapon's HitFragments (CWeapon::UpdateWeaponFX), and\n"
		"// one class's own.  Blood2/ObjectDLL/BloodModWeaponClasses.h has the rest.\n"
		+ BloodModWeaponClasses_Reference();
}

void WeaponClassesSelfTest()
{
	Unload();
	g_Retail = Literals();
	TestLiterals();
	TestKeys();
	TestRoundTrip();
	TestErrors();
}
