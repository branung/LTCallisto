// ammo.cpp: testBlood2Tables for the ammo types' table, Mod\Ammo.txt over gAmmoValues.
//
//   retail        gAmmoValues is the three tables it replaced, value for
//                 value, transcribed here from the source before the move.
//   round trip    the --dump file, loaded as a mod, changes nothing; a dump
//                 with one value moved is caught, there and only there.
//   mutation      every key of every type moves its one value, except a
//                 Pickup where there is no pickup and a Regen on any but
//                 FOCUS, which are refused by name.
//   errors        a bad file loads nothing, over a mod loaded before it.

#include "server_de.h"
#include "SharedDefs.h"
#include "AmmoValues.h"
#include "BloodModAmmo.h"
#include "harness.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{

AmmoValues g_RetailAmmo[AMMO_MAXAMMOTYPES + 1];

bool LoadAmmo(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\Ammo.txt", text, err, BloodModAmmo_Load);
}

void Unload()
{
	BloodModAmmo_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// Every differing value, "BULLET.fMax3".
std::vector<std::string> DifferAll()
{
	std::vector<std::string> out;
	char b[64];
	for (int n = AMMO_NONE; n <= AMMO_MAXAMMOTYPES; ++n)
	{
		const AmmoValues& a = gAmmoValues[n];
		const AmmoValues& r = g_RetailAmmo[n];
		const char* pName = n == AMMO_NONE ? "NONE" : BloodModAmmo_Name(n);
		for (int i = 0; i < 6; ++i)
		{
			if (a.fMax[i] != r.fMax[i]) { snprintf(b, sizeof(b), "%s.fMax%d", pName, i + 1); out.push_back(b); }
			if (a.fRegen[i] != r.fRegen[i]) { snprintf(b, sizeof(b), "%s.fRegen%d", pName, i + 1); out.push_back(b); }
		}
		if (a.fPickup != r.fPickup) out.push_back(std::string(pName) + ".fPickup");
		if (a.bEjectShell != r.bEjectShell) out.push_back(std::string(pName) + ".bEjectShell");
	}
	return out;
}

void TestRetail()
{
	printf("ammo: retail\n");
	// InventoryMgr.cpp's fMaxAmmoCount and g_fFocusRegenAmt, and SharedDefs.h's AMMOCOUNT_, as retail's
	// source had them before Shared/AmmoValues.cpp took them over
	const float kMax[AMMO_MAXAMMOTYPES + 1][6] =
	{
		{0.0f,  0.0f,   0.0f,   0.0f,   0.0f,   0.0f},
		{100.0f,200.0f, 300.0f, 400.0f, 500.0f, 600.0f},
		{50.0f,	75.0f,  100.0f, 125.0f, 150.0f, 175.0f},
		{20.0f,	40.0f,  60.0f,  80.0f,  100.0f, 120.0f},
		{20.0f,	40.0f,  60.0f,  80.0f,  100.0f, 120.0f},
		{25.0f,	50.0f,  75.0f,  100.0f, 150.0f, 200.0f},
		{5.0f,	10.0f,	20.0f,	35.0f,	50.0f,	120.0f},
		{25.0f,	25.0f,	50.0f,	75.0f,	100.0f,	125.0f},
		{100.0f,200.0f, 300.0f, 400.0f, 500.0f, 600.0f},
		{100.0f,200.0f, 300.0f, 400.0f, 500.0f, 600.0f},
		{10.0f, 10.0f,  10.0f,  10.0f,  10.0f,  10.0f},
		{10.0f, 10.0f,  10.0f,  10.0f,  10.0f,  10.0f},
		{10.0f, 10.0f,  10.0f,  10.0f,  10.0f,  10.0f},
	};
	const float kRegen[6] = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f};
	const float kPickup[AMMO_MAXAMMOTYPES + 1] =
	{
		0, AMMOCOUNT_BULLET, AMMOCOUNT_SHELL, AMMOCOUNT_BMG, AMMOCOUNT_FLARE, AMMOCOUNT_DIEBUGDIE,
		AMMOCOUNT_HOWITZER, AMMOCOUNT_FUEL, AMMOCOUNT_BATTERY, 0, 0, 0, 0
	};
	int nBad = 0;
	for (int n = 0; n <= AMMO_MAXAMMOTYPES; ++n)
	{
		const AmmoValues& v = gAmmoValues[n];
		// Weapon.cpp's m_bEjectShell: m_nAmmoType == AMMO_BULLET || AMMO_SHELL.
		bool b = v.fPickup == kPickup[n] && !!v.bEjectShell == (n == AMMO_BULLET || n == AMMO_SHELL);
		for (int i = 0; i < 6; ++i)
			b = b && v.fMax[i] == kMax[n][i] && v.fRegen[i] == (n == AMMO_FOCUS ? kRegen[i] : 0.0f);
		if (!b && ++nBad <= 4) printf("        ammo %d differs\n", n);
	}
	Check(nBad == 0, "gAmmoValues is retail's fMaxAmmoCount, AMMOCOUNT_, g_fFocusRegenAmt and casings, all 13 rows");
	Check(AmmoHasPickup(AMMO_BULLET) && AmmoHasPickup(AMMO_BATTERY) && !AmmoHasPickup(AMMO_FOCUS) &&
		  !AmmoHasPickup(AMMO_PROXIMITYBOMB) && AmmoRegenerates(AMMO_FOCUS) && !AmmoRegenerates(AMMO_BULLET),
		  "BULLET .. BATTERY have a pickup, and FOCUS alone regenerates");
	bool bNames = true;
	for (int n = AMMO_BULLET; n <= AMMO_MAXAMMOTYPES; ++n)
		bNames = bNames && BloodModAmmo_Name(n) && BloodModAmmo_Find(BloodModAmmo_Name(n)) == n;
	Check(bNames && BloodModAmmo_Find("focus") == AMMO_FOCUS && BloodModAmmo_Find("ROCKET") == AMMO_NONE &&
		  !BloodModAmmo_Name(AMMO_NONE), "every AMMO_ id has a name that finds it, without case; NONE has none");
}

void TestRoundTrip()
{
	printf("ammo: round trip\n");
	std::string err;
	Unload();
	const std::string dump = AmmoDump();
	Check(LoadAmmo(dump, err) && DifferAll().empty() && BloodModAmmo_Summary().empty(),
		  "the dump loads as a mod and changes nothing" + (err.empty() ? std::string() : ": " + err));
	std::string bent = dump;
	const size_t at = bent.find("[SHELL]\n");
	const size_t k = bent.find("\nMax3 = ", at);
	const size_t e = bent.find('\n', k + 1);
	bent.replace(k, e - k, "\nMax3 = 101.0");
	const bool bLoaded = LoadAmmo(bent, err);
	const std::vector<std::string> got = DifferAll();
	Check(bLoaded && got.size() == 1 && got[0] == "SHELL.fMax3",
		  "calibration: a dump with SHELL's Max3 moved is caught there and only there (" + Join(got) + ")");
	Unload();
}

void TestMutation()
{
	printf("ammo: mutation\n");
	std::string err;
	int nCases = 0, nBad = 0, nRefused = 0;
	for (int n = AMMO_BULLET; n <= AMMO_MAXAMMOTYPES; ++n)
		for (int f = 0; BloodModAmmo_Key(f); ++f)
		{
			const std::string key = BloodModAmmo_Key(f);
			const std::string sNew = Other(key, BloodModAmmo_Value(n, f, true));
			const std::string text = std::string("[") + BloodModAmmo_Name(n) + "]\n" + key + " = " + sNew + "\n";
			const bool bInert = (key == "Pickup" && !AmmoHasPickup(n)) ||
								(key.compare(0, 5, "Regen") == 0 && !AmmoRegenerates(n));
			++nCases;
			err.clear();
			const bool bLoaded = LoadAmmo(text, err);
			const std::vector<std::string> d = DifferAll();
			bool bOk;
			if (bInert)
			{
				bOk = !bLoaded && d.empty() && err.find(std::string("[") + BloodModAmmo_Name(n) + "]:" +
														  (key == "Pickup" ? "Pickup" : "Regen")) != std::string::npos;
				nRefused += bOk;
			}
			else
				bOk = bLoaded && d.size() == 1 && fabs(atof(BloodModAmmo_Value(n, f).c_str()) - atof(sNew.c_str())) < 1e-4 &&
					  BloodModAmmo_Summary().compare(0, strlen(BloodModAmmo_Name(n)) + 2 + key.size(),
													 std::string(BloodModAmmo_Name(n)) + ": " + key) == 0;
			if (!bOk && ++nBad <= 6)
				printf("        [%s] %s = %s: loaded %d, moved %s, %s\n", BloodModAmmo_Name(n), key.c_str(), sNew.c_str(),
					   (int)bLoaded, Join(d).c_str(), err.c_str());
		}
	char b[200];
	snprintf(b, sizeof(b), "%d single-key overrides: each moves its one value, and the %d that nothing would read "
			 "(Pickup without a pickup, Regen but on FOCUS) are refused", nCases, nRefused);
	Check(nBad == 0 && nRefused == 4 + 6 * 11, b);
	Unload();
}

void TestErrors()
{
	printf("ammo: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a section naming no ammo",		"[ROCKET]\nMax1 = 5.0\n",		"is not a Blood 2 ammo type" },
		{ "NONE",							"[NONE]\nMax1 = 5.0\n",			"is not a Blood 2 ammo type" },
		{ "a key it has not",				"[BULLET]\nMax7 = 5.0\n",		"has no field \"Max7\"" },
		{ "a negative cap",				"[BULLET]\nMax1 = -1.0\n",		"Max1 is out of range" },
		{ "a pickup for FOCUS",			"[FOCUS]\nPickup = 5.0\n",		"[FOCUS]:Pickup: it has no ammo pickup" },
		{ "a regen for BULLET",			"[BULLET]\nRegen2 = 1.0\n",		"[BULLET]:Regen: only FOCUS regenerates" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadAmmo("[FUEL]\nMax1 = 300.0\n", err);
		const bool bBefore = DifferAll().size() == 1;
		err.clear();
		const bool bLoaded = LoadAmmo(bad[i].text, err);
		Check(bBefore && !bLoaded && DifferAll().empty() && err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	Unload();
}

} // namespace

std::string AmmoDump()
{
	return
		"// Blood II's ammo types, every value retail ships: the reference for a\n"
		"// mod's Mod\\Ammo.txt. GENERATED by `testBlood2Tables --dump ammo` from\n"
		"// Blood2/Shared/AmmoValues.cpp; do not edit, regenerate. testBlood2Tables\n"
		"// proves that loading this file as a mod changes nothing.\n"
		"//\n"
		"// A section is the type's AMMO_ name (Shared/SharedDefs.h) without the\n"
		"// prefix.  Max1 .. Max6 are the most a player carries at strength 1 .. 6\n"
		"// (magic, for FOCUS); Pickup is what the type's ammo pickup gives, and\n"
		"// only BULLET .. BATTERY have one; Regen1 .. Regen6 are what FOCUS\n"
		"// regains a second at magic 1 .. 6, and no other type regenerates.\n"
		"// EjectShell TRUE makes a weapon firing the type eject a casing, and\n"
		"// only then show its muzzle smoke and light.  A mod names only what it\n"
		"// changes:\n"
		"//\n"
		"//     [BULLET]\n"
		"//     Max1 = 200.0\n"
		"//\n"
		"// Blood2/Shared/BloodModAmmo.h has the rest.\n"
		+ BloodModAmmo_Reference();
}

void AmmoSelfTest()
{
	memcpy(g_RetailAmmo, gAmmoValues, sizeof(g_RetailAmmo));
	TestRetail();
	TestRoundTrip();
	TestMutation();
	TestErrors();
}
