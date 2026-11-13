// rules.cpp: testBlood2Tables for Mod\Rules.txt (Shared/BloodModRules.h).
//
//   retail        with no mod gRules holds the constants the code held,
//                 transcribed here from retail's source.
//   keys          every key, set alone to another value, loads and changes
//                 that value and no other; under any other section it is
//                 refused, naming its own.
//   round trip    the --dump file loads as a mod and changes nothing.
//   errors        a bad file loads nothing, over a mod loaded before it.

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "BloodModRules.h"
#include "harness.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{

bool LoadRules(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\Rules.txt", text, err, BloodModRules_Load);
}

void Unload()
{
	BloodModRules_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

// Every key's value as the file writes it, in one string.
std::string All()
{
	std::string s;
	for (int i = 0; BloodModRules_Key(i); ++i)
		s += std::string(BloodModRules_Key(i)) + "=" + BloodModRules_Value(i) + ";";
	return s;
}

// The keys whose current value is not retail's.
std::vector<std::string> Moved()
{
	std::vector<std::string> out;
	for (int i = 0; BloodModRules_Key(i); ++i)
		if (BloodModRules_Value(i) != BloodModRules_Value(i, true)) out.push_back(BloodModRules_Key(i));
	return out;
}

std::string OtherRule(const std::string& key, const std::string& v)
{
	if (key == "SpawnItems")	return v == "\"FLASHLIGHT\"" ? "\"MEDKIT\"" : "\"FLASHLIGHT\"";
	if (key == "SpawnAmmo")		return v == "\"SHELL 20\"" ? "\"BULLET 50\"" : "\"SHELL 20\"";
	if (key == "BurnFX" || key == "SmokeFX")	return "\"FLAMING_2 FLAMING_3\"";
	if (key == "LiquidMark")	return v == "\"\"" ? "\"sprites/ripple.spr\"" : "\"\"";
	if (key == "VersionText")	return v == "\"\"" ? "\"MOD v1.0\"" : "\"\"";
	if (key == "MoreWeapons")	return v == "\"SHOTGUN\"" ? "\"BERETTA\"" : "\"SHOTGUN\"";
	if (key == "AllItems")		return v == "\"MEDKIT\"" ? "\"FLASHLIGHT\"" : "\"MEDKIT\"";
	if (v == "TRUE")	return "FALSE";
	if (v == "FALSE")	return "TRUE";
	if (key.find("Chance") != std::string::npos) return v == "50" ? "40" : "50";
	if (key == "InvisibleAlpha")	return "0.5";
	// A minimum moves down and a maximum up, so a pair never runs backwards;
	// an integer is written without a point.
	const bool bInt = v.find('.') == std::string::npos;
	const bool bMin = key.size() > 3 && key.compare(key.size() - 3, 3, "Min") == 0;
	const bool bMax = key.size() > 3 && key.compare(key.size() - 3, 3, "Max") == 0;
	if (bMin)	return bInt ? "0" : "0.0";
	if (bMax)	return bInt ? "5" : "99.0";
	if (bInt)	return v == "5" ? "6" : "5";
	return "2.5";
}

void TestRetail()
{
	printf("rules: retail\n");
	Unload();
	// The constants as retail's source had them: PlayerObj.cpp's ALPHA_INVISIBLE, DIST_USE and
	// MAX_IDLE_TIME and InitialUpdate's AddAmmo(AMMO_BULLET, 50); GameInvItems.h's two charge times;
	// UpdateWeaponFX's `fDamage > 25.0f`; and corpse.cpp's FLAMING_2, SMOKING_2, IsDead gate and limb
	// chances (the head unconditional, the arms 80, the legs 60).
	Check(gRules.fInvisibleAlpha == 0.08f && gRules.fUseDistance == 82.0f && gRules.fIdleVoiceTime == 60.0f &&
		  !strcmp(gRules.szSpawnItems, "") && !strcmp(gRules.szSpawnAmmo, "BULLET 50") &&
		  gRules.fFlashlightChargeTime == 120.0f && gRules.fGogglesChargeTime == 30.0f &&
		  gRules.fBloodSpurtDamage == 25.0f && !strcmp(gRules.szBurnFX, "FLAMING_2") &&
		  !strcmp(gRules.szSmokeFX, "SMOKING_2") && !gRules.bGibOnExplosionOnly && gRules.nGibHeadChance == 100 &&
		  gRules.nGibArmChance == 80 && gRules.nGibLegChance == 60,
		  "with no mod gRules is the retail code's constants: " + All());
	// The client's, from retail's ClientShellDLL: BloodClientShell.cpp's BOB/SWAY_WALKING and
	// shake's * 3.0f, ViewWeapon's idle delays, MoveMgr's LADDER_STOP_TIME and swim flags,
	// FlashlightFX's 0.40f, WeaponFX's impact literals, SFXMgr's gib multipliers and list sizes.
	const int kCaps[8] = { 300, 300, 80, 80, 80, 80, 100, 80 };
	bool bCaps = true;
	for (int i = 0; i < 8; ++i) bCaps = bCaps && gRules.nCaps[i] == kCaps[i];
	Check(gRules.fBobVertical == 24.0f && gRules.fBobHorizontal == 16.0f && gRules.fSwayVertical == 32.0f &&
		  gRules.fSwayHorizontal == 80.0f && gRules.fShakeScale == 3.0f && gRules.fIdleDelayMin == 20.0f &&
		  gRules.fIdleDelayMax == 60.0f && gRules.fLadderStopTime == 0.2f && !gRules.bSwimJumpRepeat &&
		  gRules.bSurfaceFloat && gRules.fFlashlightRadius == 0.40f && gRules.fSplatScaleMin == 0.05f &&
		  gRules.fSplatScaleMax == 0.15f && gRules.nSpurtEmittersMin == 1 && gRules.nSpurtEmittersMax == 1 &&
		  gRules.fBigMetalSparkTime == 2.0f && gRules.fMetalSparkTime == 0.5f &&
		  gRules.fBloodSparkEmissionRadius == 5.0f && gRules.fBloodSparkRadius == 200.0f &&
		  gRules.nFragmentsMin == 1 && gRules.nFragmentsMax == 3 && gRules.bBubblesInLiquid &&
		  gRules.fSmokeDelay == 0.0f && gRules.fSmokeScale == 0.35f && gRules.fBubbleGravity == 50.0f &&
		  !strcmp(gRules.szLiquidMark, "") && gRules.nFleshGibChance == 20 && gRules.nFleshGibMore == 2 &&
		  gRules.nFleshGibScale == 1 && bCaps,
		  "with no mod the client's constants are the retail code's");
	// And its features: wonky vision on, a dual alt fired as primary, the tabs hidden on a reset,
	// the build's version line, the multiplayer worlds listed, and the add-on's logos straight to the
	// menu.
	Check(gRules.bWonkyVision && gRules.bDualAltAsPrimary && !gRules.bWeaponTabsOnReset &&
		  !strcmp(gRules.szVersionText, "") && gRules.bCustomLevelsMultiplayer && !gRules.bAddonIntroMovie,
		  "with no mod the client's features are retail's");
	std::vector<int> fx;
	Check(BloodModRules_ObjFXList("FLAMING_2, smoking_3", fx) && fx.size() == 2 && fx[0] == OBJFX_FLAMING_2 &&
		  fx[1] == OBJFX_SMOKING_3 && BloodMod_ObjFX("ELECTRIC_1") == OBJFX_ELECTRIC_1 &&
		  !strcmp(BloodMod_ObjFXName(OBJFX_VOODOO_6), "VOODOO_6") && BloodMod_ObjFX("BOGUS") < 0,
		  "an OBJFX_ list reads by name without case, and each id has its name");
}

void TestKeys()
{
	printf("rules: keys\n");
	std::vector<std::string> sections;
	for (int i = 0; BloodModRules_Key(i); ++i)
		if (sections.empty() || sections.back() != BloodModRules_Section(i)) sections.push_back(BloodModRules_Section(i));
	int nCases = 0, nBad = 0, nRefused = 0;
	for (int i = 0; BloodModRules_Key(i); ++i)
		for (size_t s = 0; s < sections.size(); ++s)
		{
			const std::string key = BloodModRules_Key(i), v = OtherRule(key, BloodModRules_Value(i, true));
			const bool bOwn = sections[s] == BloodModRules_Section(i);
			const std::string text = "[" + sections[s] + "]\n" + key + " = " + v + "\n";
			std::string err;
			++nCases;
			const bool bLoaded = LoadRules(text, err);
			const std::vector<std::string> d = Moved();
			bool bOk;
			if (bOwn)
				bOk = bLoaded && d.size() == 1 && d[0] == key && BloodModRules_Value(i) == v &&
					  BloodModRules_Summary() == std::string(BloodModRules_Section(i)) + ": " + key + " " + v;
			else
			{
				bOk = !bLoaded && d.empty() &&
					  err.find(std::string("it is [") + BloodModRules_Section(i) + "]'s") != std::string::npos;
				nRefused += bOk;
			}
			if (!bOk && ++nBad <= 6)
				printf("        [%s] %s = %s: loaded %d, moved %d, \"%s\" %s\n", sections[s].c_str(), key.c_str(), v.c_str(),
					   (int)bLoaded, (int)d.size(), BloodModRules_Summary().c_str(), err.c_str());
		}
	char b[220];
	snprintf(b, sizeof(b), "%d single-field overrides over every key and section: each moves its one value under its "
			 "own section, and the %d elsewhere are refused naming it", nCases, nRefused);
	Check(nBad == 0, b);
	Unload();
}

void TestRoundTrip()
{
	printf("rules: round trip\n");
	std::string err;
	Unload();
	const std::string dump = RulesDump();
	Check(LoadRules(dump, err) && Moved().empty() && BloodModRules_Summary().empty(),
		  "the dump loads as a mod and changes nothing" + (err.empty() ? std::string() : ": " + err));
	std::string bent = dump;
	const size_t at = bent.find("GibLegChance = 60");
	if (at != std::string::npos) bent.replace(at, strlen("GibLegChance = 60"), "GibLegChance = 0");
	const bool bLoaded = LoadRules(bent, err);
	const std::vector<std::string> d = Moved();
	Check(bLoaded && d.size() == 1 && d[0] == "GibLegChance" && gRules.nGibLegChance == 0,
		  "calibration: a dump with GibLegChance moved is caught there and only there");
	Unload();
}

void TestErrors()
{
	printf("rules: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a section there is not",		"[Weapons]\nUseDistance = 100.0\n",			"[Weapons] is not a section" },
		{ "a key there is not",			"[Player]\nJumpHeight = 1.0\n",				"[Player] has no key \"JumpHeight\" (" },
		{ "a negative distance",		"[Player]\nUseDistance = -1.0\n",			"[Player]:UseDistance is out of range" },
		{ "an alpha above 1",			"[Player]\nInvisibleAlpha = 1.5\n",			"[Player]:InvisibleAlpha is out of range" },
		{ "a chance above 100",			"[Corpse]\nGibArmChance = 101\n",			"[Corpse]:GibArmChance is out of range" },
		{ "a chance as a float",		"[Corpse]\nGibArmChance = 50.5\n",			"GibArmChance" },
		{ "an item there is not",		"[Player]\nSpawnItems = \"JETPACK\"\n",		"[Player]:SpawnItems: \"JETPACK\" is no item" },
		{ "ammo without a count",		"[Player]\nSpawnAmmo = \"BULLET\"\n",		"[Player]:SpawnAmmo: \"BULLET\" wants a count" },
		{ "an effect there is not",		"[Corpse]\nBurnFX = \"FLAMING_9\"\n",		"[Corpse]:BurnFX: \"FLAMING_9\" is not an OBJFX_ name" },
		{ "no effect",					"[Corpse]\nSmokeFX = \"\"\n",				"[Corpse]:SmokeFX: it names no effect" },
		{ "a number for a flag",		"[Corpse]\nGibOnExplosionOnly = 1\n",		"GibOnExplosionOnly" },
		{ "a ladder that never stops",	"[Movement]\nLadderStopTime = 0.0\n",		"[Movement]:LadderStopTime is out of range" },
		{ "a pair run backwards",		"[Impacts]\nFragmentsMin = 4\n",				"[Impacts]:FragmentsMin is above FragmentsMax" },
		{ "a cap past its limit",		"[Effects]\nGibCap = 5000\n",				"[Effects]:GibCap is out of range" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadRules("[Items]\nGogglesChargeTime = 900.0\n", err);
		const bool bBefore = Moved().size() == 1;
		err.clear();
		const bool bLoaded = LoadRules(bad[i].text, err);
		Check(bBefore && !bLoaded && Moved().empty() && err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	// A mod's string outlives the next load: gRules keeps the pointer, as the player's InitialUpdate
	// reads it every world.
	std::string err;
	LoadRules("[Player]\nSpawnItems = \"FLASHLIGHT MEDKIT\"\n", err);
	const char* p = gRules.szSpawnItems;
	LoadRules("[Player]\nUseDistance = 122.0\n", err);
	Check(!strcmp(p, "FLASHLIGHT MEDKIT") && !strcmp(gRules.szSpawnItems, ""),
		  "a mod's list is kept after the next load, which starts from retail's");
	Unload();
}

} // namespace

std::string RulesDump()
{
	return
		"// Blood II's loose constants, every value retail ships: the reference\n"
		"// for a mod's Mod\\Rules.txt. GENERATED by `testBlood2Tables --dump\n"
		"// rules` from Blood2/Shared/BloodModRules.cpp; do not edit, regenerate.\n"
		"// testBlood2Tables proves that loading this file as a mod changes nothing.\n"
		"//\n"
		"// A section is an area of the game, and each has its own keys:\n"
		"// [Player] the player's invisibility alpha, use reach, seconds idle\n"
		"// before the idle line, and the items (INV_ names) and ammo (AMMO_ name\n"
		"// and count pairs) it gets on entering a world; [Items] the flashlight's\n"
		"// and goggles' seconds of charge; [Impacts] the damage above which a\n"
		"// flesh hit always spurts blood; [Corpse] the effects a burning and a\n"
		"// smoking corpse show, one of each list at random, whether only an\n"
		"// explosion gibs a corpse, and the percent chance of each limb a gib\n"
		"// throws.  The client reads [Camera] (bob, sway, shake), [ViewWeapon]\n"
		"// (the idle delay), [Movement] (ladders and swimming), [Items]'\n"
		"// FlashlightRadius, the rest of [Impacts] (WeaponFX.cpp's numbers) and\n"
		"// [Effects] (gib bursts and how many effects live at once), [HUD] and\n"
		"// [Menus] (the version line, the custom-level list, the intro); both\n"
		"// modules read SwimJumpRepeat and LiquidMark.  A Min above its Max is\n"
		"// refused.  A mod names only what it changes:\n"
		"//\n"
		"//     [Player]\n"
		"//     UseDistance = 122.0\n"
		"//\n"
		"// Blood2/Shared/BloodModRules.h has the rest.\n"
		+ BloodModRules_Reference();
}

void RulesSelfTest()
{
	TestRetail();
	TestKeys();
	TestRoundTrip();
	TestErrors();
}
