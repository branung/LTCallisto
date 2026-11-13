// explosions.cpp: testBlood2Tables for Mod\Explosions.txt (Shared/BloodModExplosions.h).
//
//   names         every retail EXP_ name finds its id, and a mod's new names
//                 number from BLOODMOD_EXP_FIRST_NEW in file order.
//   pieces        each kind takes its own fields and refuses another kind's;
//                 numbers, ranges, vectors, paths and names read, and a
//                 piece's text reads back as itself.
//   file          pieces add to retail's per detail level or replace them; a
//                 new explosion is usable by name in the projectile table.
//   errors        a bad file loads nothing, over a mod loaded before it.

#include "server_de.h"
#include "SharedDefs.h"
#include "SFXMsgIds.h"
#include "BloodModExplosions.h"
#include "BloodModProjectiles.h"
#include "harness.h"

#include <stdio.h>
#include <string.h>
#include <string>

namespace
{

bool LoadExp(const std::string& text, std::string& err)
{
	return MountAndLoad("Mod\\Explosions.txt", text, err, BloodModExplosions_Load);
}

void Unload()
{
	BloodModExplosions_Clear();
	BloodModProjectiles_Clear();
	LT1Mod::SetActive(LT1Mod::State());
}

void TestNames()
{
	printf("explosions: names\n");
	Unload();
	// SFXMsgIds.h's values.
	Check(BloodModExplosion_Find("DEFAULT_SMALL") == EXP_DEFAULT_SMALL && BloodModExplosion_Find("rift_2") == EXP_RIFT_2 &&
		  BloodModExplosion_Find("NAGA_POUND_GROUND") == EXP_NAGA_POUND_GROUND &&
		  !strcmp(BloodModExplosion_Name(EXP_HOWITZER_MINI), "HOWITZER_MINI") && BloodModExplosion_Find("SPARKLER") < 0 &&
		  !BloodModExplosion_Name(BLOODMOD_EXP_FIRST_NEW) && BloodModExplosion_NewCount() == 0 &&
		  !BloodModExplosion_Recipe(EXP_GRENADE, 2),
		  "with no mod every retail EXP_ name finds its id, without case, and there is no new one and no recipe");
#ifdef _ADD_ON
	Check(BloodModExplosion_Find("FLAYER_SHATTER") == EXP_FLAYER_SHATTER, "the add-on's names are there with _ADD_ON");
#endif
}

void TestPieces()
{
	printf("explosions: pieces\n");
	// One field each kind has, and one it has not.
	const char* const kGood[BMX_KINDS] =
	{
		"Model Model=Models\\a.abc Scale1=<1,1,1> RandomRot=1",
		"Sprite Sprite=Sprites\\a.spr Align=2 WaveForm=1",
		"Ring Particle=SpriteTextures\\p.dtx Particles=32 Color=<1,0.5,0>",
		"Light Color1=<1,1,1> Radius1=150 Radius2=0",
		"Frag Model=Models\\g.abc Spread=40..80 TrailType=SMOKETRAIL_1 SpawnType=GRENADE",
		"Wave Wave=Sprites\\w.spr Delay=0.1",
		"Flame Particle=SpriteTextures\\f.dtx LifeTimes=<0.5,1,1.5> RampFlags=3",
	};
	const char* const kBad[BMX_KINDS] =
	{
		"Model Sprite=x", "Sprite Model=x", "Ring Radius1=1", "Light Normal=1", "Frag Wave=x", "Wave Particles=1",
		"Flame Duration=1",
	};
	int nGood = 0, nBad = 0;
	for (int k = 0; k < BMX_KINDS; ++k)
	{
		BloodModFxPiece p;
		std::string why;
		if (BloodModExplosion_ParsePiece(kGood[k], p, why) && p.nKind == k) ++nGood;
		else printf("        %s: %s\n", kGood[k], why.c_str());
		if (!BloodModExplosion_ParsePiece(kBad[k], p, why) && why.find("is no ") != std::string::npos) ++nBad;
		else printf("        %s accepted\n", kBad[k]);
	}
	Check(nGood == BMX_KINDS && nBad == BMX_KINDS, "each kind takes its own fields and refuses another kind's");

	BloodModFxPiece p;
	std::string why;
	const bool bRead = BloodModExplosion_ParsePiece(
		"frag offset=10 Normal=1 Count=5..10 Scale=<0.75, 0.75, 1> Spread=40..80 Velocity=20..40 "
		"TrailType=smoketrail_1 Model=Models\\Gibs\\gibmetal2.abc", p, why);
	Check(bRead && p.nKind == BMX_FRAG && p.v[BMF_OFFSET].lo[0] == 10.0f && p.v[BMF_COUNT].lo[0] == 5.0f &&
		  p.v[BMF_COUNT].hi[0] == 10.0f && p.v[BMF_SCALE].lo[2] == 1.0f && p.v[BMF_SPREAD].hi[0] == 80.0f &&
		  p.v[BMF_TRAILTYPE].nId == OBJFX_SMOKETRAIL_1 && p.v[BMF_MODEL].s == "Models\\Gibs\\gibmetal2.abc" &&
		  !p.v[BMF_DURATION].bSet,
		  "a piece reads kind and fields without case, ranges, a vector with spaces, a name and a path: " + why);
	const std::string text = BloodModExplosion_PieceText(p);
	BloodModFxPiece q;
	Check(BloodModExplosion_ParsePiece(text.c_str(), q, why) && BloodModExplosion_PieceText(q) == text &&
		  text == "Frag Offset=10.0 Normal=1.0 Count=5.0..10.0 Model=Models\\Gibs\\gibmetal2.abc Velocity=20.0..40.0 "
				  "Scale=<0.75,0.75,1.0> Spread=40.0..80.0 TrailType=SMOKETRAIL_1",
		  "and its text reads back as itself: " + text);

	struct Bad { const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "",									"is empty" },
		{ "Spark Duration=1",					"\"Spark\" is no kind" },
		{ "Light Radius1",						"(Field=value)" },
		{ "Light Radius1=big",					"Radius1: \"big\" is not a number" },
		{ "Light Color1=<1,1>",					"is not a vector" },
		{ "Sprite Count=300",					"is not a count" },
		{ "Sprite Count=1.5",					"is not a count" },
		{ "Frag SpawnType=BOOM",				"is not an EXP_ name" },
		{ "Frag TrailType=SMOKE",				"is not an OBJFX_ name" },
		{ "Light Radius1=1 Radius1=2",			"given twice" },
	};
	int nRefused = 0;
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		if (!BloodModExplosion_ParsePiece(bad[i].text, p, why) && why.find(bad[i].expect) != std::string::npos) ++nRefused;
		else printf("        \"%s\": %s\n", bad[i].text, why.c_str());
	}
	char b[100];
	snprintf(b, sizeof(b), "%d bad pieces are refused, each saying why", nRefused);
	Check(nRefused == (int)(sizeof(bad) / sizeof(bad[0])), b);
}

void TestFile()
{
	printf("explosions: file\n");
	std::string err;
	const std::string mod =
		"[GRENADE_HIGH]\nPiece1 = \"Sprite Sprite=Sprites\\smokepuff3.spr Offset=15 Normal=1 Duration=6\"\n\n"
		"[RIFT_1]\nReplace = TRUE\nPiece2 = \"Light Color1=<1,0,0> Radius1=100\"\nPiece1 = \"Light Color1=<0,0,1> Radius1=50\"\n\n"
		"[SPARKLER]\nPiece1 = \"Frag Count=3 SpawnType=FIZZ\"\n\n"
		"[FIZZ_LOW]\nPiece1 = \"Light Radius1=10\"\n";
	const bool bLoaded = LoadExp(mod, err);
	const BloodModFxRecipe* g2 = BloodModExplosion_Recipe(EXP_GRENADE, 2);
	const BloodModFxRecipe* r0 = BloodModExplosion_Recipe(EXP_RIFT_1, 0);
	const BloodModFxRecipe* s1 = BloodModExplosion_Recipe(BLOODMOD_EXP_FIRST_NEW, 1);
	Check(bLoaded && g2 && !g2->bReplace && g2->pieces.size() == 1 && !BloodModExplosion_Recipe(EXP_GRENADE, 1) &&
		  r0 && r0->bReplace && r0->pieces.size() == 2 && r0->pieces[0].v[BMF_RADIUS1].lo[0] == 50.0f &&
		  BloodModExplosion_Recipe(EXP_RIFT_1, 2),
		  "a level's section adds at that level alone, an all-levels one at all three, and pieces go by number: " + err);
	Check(BloodModExplosion_NewCount() == 2 && BloodModExplosion_Find("sparkler") == BLOODMOD_EXP_FIRST_NEW &&
		  BloodModExplosion_Find("FIZZ") == BLOODMOD_EXP_FIRST_NEW + 1 && s1 &&
		  s1->pieces[0].v[BMF_SPAWNTYPE].nId == BLOODMOD_EXP_FIRST_NEW + 1 &&
		  BloodModExplosion_Recipe(BLOODMOD_EXP_FIRST_NEW + 1, 0) && !BloodModExplosion_Recipe(BLOODMOD_EXP_FIRST_NEW + 1, 2),
		  "new explosions number from BLOODMOD_EXP_FIRST_NEW in file order, and a piece can name one further down");
	Check(BloodModExplosions_Summary() ==
			  "GRENADE_HIGH: 1 piece\nRIFT_1_LOW: 2 pieces, replacing retail's\nRIFT_1_MEDIUM: 2 pieces, replacing retail's\n"
			  "RIFT_1_HIGH: 2 pieces, replacing retail's\nSPARKLER_LOW: new, 1 piece\n"
			  "SPARKLER_MEDIUM: new, 1 piece\nSPARKLER_HIGH: new, 1 piece\nFIZZ_LOW: new, 1 piece",
		  "the summary says each level's pieces: " + BloodModExplosions_Summary());
	// The projectile table takes the new name as it takes a retail one.
	err.clear();
	const bool bProj = MountAndLoad("Mod\\Projectiles.txt", "[CTimeBomb]\nExplosionType = \"SPARKLER\"\n", err,
									BloodModProjectiles_Load);
	Check(bProj, "Mod\\Projectiles.txt takes a new explosion's name: " + err);
	Unload();
}

void TestRoundTrip()
{
	printf("explosions: round trip\n");
	std::string err;
	Unload();
	Check(LoadExp(ExplosionsDump(), err) && BloodModExplosions_Summary().empty() && BloodModExplosion_NewCount() == 0,
		  "the dump loads as a mod and changes nothing" + (err.empty() ? std::string() : ": " + err));
	Unload();
}

void TestErrors()
{
	printf("explosions: errors\n");
	struct Bad { const char* label; const char* text; const char* expect; };
	const Bad bad[] =
	{
		{ "a key there is not",			"[GRENADE]\nSize = 2.0\n",									"has no key \"Size\"" },
		{ "a bad piece",				"[GRENADE]\nPiece3 = \"Light Radius1=x\"\n",				"[GRENADE]:Piece3: Radius1" },
		{ "Replace on a new one",		"[ZAP]\nReplace = TRUE\n",									"ZAP is a new explosion" },
		{ "all levels and one",			"[GRENADE]\nPiece1 = \"Light Radius1=1\"\n[GRENADE_LOW]\nPiece1 = \"Light Radius1=1\"\n",
																								"given twice for a detail level" },
		{ "a new name too long",		"[ZAP_ZAP_ZAP_ZAP_ZAP_ZAP_ZAP_ZAP_ZAP]\nPiece1 = \"Light Radius1=1\"\n",	"a new explosion's name" },
		{ "Replace as a number",		"[GRENADE]\nReplace = 1\n",								"Replace" },
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
	{
		std::string err;
		LoadExp("[GRENADE_HIGH]\nPiece1 = \"Light Radius1=5\"\n[OLDNEW]\nPiece1 = \"Light Radius1=5\"\n", err);
		const bool bBefore = BloodModExplosion_Recipe(EXP_GRENADE, 2) && BloodModExplosion_NewCount() == 1;
		err.clear();
		const bool bLoaded = LoadExp(bad[i].text, err);
		Check(bBefore && !bLoaded && !BloodModExplosion_Recipe(EXP_GRENADE, 2) && BloodModExplosion_NewCount() == 0 &&
			  err.find(bad[i].expect) != std::string::npos,
			  std::string(bad[i].label) + " loads nothing, over a mod loaded before it: " + err);
	}
	Unload();
}

} // namespace

std::string ExplosionsDump()
{
	return
		"// Blood II's explosions, as a mod's Mod\\Explosions.txt names them.\n"
		"// GENERATED by `testBlood2Tables --dump explosions` from\n"
		"// Blood2/Shared/BloodModExplosions.cpp; do not edit, regenerate.\n"
		"//\n"
		"// Retail's explosions are code (ClientShellDLL/ExplosionFX.cpp), so\n"
		"// they are listed by name, not by piece.  A section is one's name with\n"
		"// _HIGH, _MEDIUM or _LOW for a detail level, or none for all three; its\n"
		"// Piece1 .. Piece64 are drawn after retail's, or instead with Replace =\n"
		"// TRUE.  A name not below is a new explosion, which a projectile, weapon\n"
		"// or script then names.  A piece is a kind and Field=value pairs:\n"
		"//\n"
		"//     [GRENADE_HIGH]\n"
		"//     Piece1 = \"Sprite Sprite=Sprites\\smokepuff3.spr Offset=15 Normal=1 Duration=6 Alpha=0.85\"\n"
		"//\n"
		"// Every kind also takes Offset, Normal (but a Light), NormalVector and\n"
		"// Count.  A value is a number, a..b, <x,y,z>, a path, or for SpawnType\n"
		"// and TrailType an EXP_ or OBJFX_ name.  Blood2/Shared/BloodModExplosions.h\n"
		"// has the rest.\n"
		+ BloodModExplosions_Reference();
}

void ExplosionsSelfTest()
{
	TestNames();
	TestPieces();
	TestFile();
	TestRoundTrip();
	TestErrors();
}
