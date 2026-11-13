// BloodModLasers.cpp: see BloodModLasers.h

#include "server_de.h"
#include "SFXMsgIds.h"
#include "BloodModLasers.h"
#include "BloodModTable.h"

#include <stddef.h>
#include <string>

using namespace BloodMod;

namespace
{

#define L(x) { LASER_##x, #x }
const Entry kNames[] =
{
	L(RED_SMALL), L(RED_LARGE), L(GREEN_SMALL), L(GREEN_LARGE), L(BLUE_SMALL), L(BLUE_LARGE),
	L(WHITE_SMALL), L(WHITE_LARGE), L(YELLOW_SMALL), L(YELLOW_LARGE), L(ORANGE_SMALL), L(ORANGE_LARGE),
	L(PURPLE_SMALL), L(PURPLE_LARGE),
};
#undef L
const int kNumNames = (int)(sizeof(kNames) / sizeof(kNames[0]));

// CLaserBeamFX::SetupBeam's two switches, transcribed: a small beam's shape, a large one's, and
// each color's skin.
#define SMALL(skin)	{ 0.05f, 1.0f, 0.5f, 0.25f, 0.25f, 0.5f, 0, (char*)"Models\\Ammo\\beam.abc", (char*)"Skins\\Ammo\\" skin }
#define LARGE(skin)	{ 0.1f, 2.5f, 1.0f, 0.35f, 0.35f, 0.5f, 0, (char*)"Models\\Ammo\\beam.abc", (char*)"Skins\\Ammo\\" skin }
LaserRecipe g_Lasers[BLOODMOD_LASER_COUNT] =
{
	SMALL("beamred.dtx"), LARGE("beamred.dtx"), SMALL("beamgreen.dtx"), LARGE("beamgreen.dtx"),
	SMALL("beamblue.dtx"), LARGE("beamblue.dtx"), SMALL("beamwhite.dtx"), LARGE("beamwhite.dtx"),
	SMALL("beamyellow.dtx"), LARGE("beamyellow.dtx"), SMALL("beamorange.dtx"), LARGE("beamorange.dtx"),
	SMALL("beampurple.dtx"), LARGE("beampurple.dtx"),
};
#undef SMALL
#undef LARGE

#define F(key, member, kind) { key, 0, offsetof(LaserRecipe, member), kind, 0 }
const Field kFields[] =
{
	F("MinScale", fMinScale, K_FLOAT), F("MaxScale", fMaxScale, K_FLOAT), F("Duration", fDuration, K_FLOAT),
	F("ScaleTime", fScaleTime, K_FLOAT), F("FadeTime", fFadeTime, K_FLOAT), F("Alpha", fAlpha, K_FLOAT),
	F("WaveForm", bWaveForm, K_BOOL), F("Model", szModel, K_STRING), F("Skin", szSkin, K_STRING),
};
#undef F
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const Array kArrays[] = { { &g_Lasers[0], sizeof(LaserRecipe) } };

Table g_Table("Mod\\Lasers.txt", "a Blood 2 laser", "the LASER_ names without the prefix",
			  "Blood2/ModReference/Lasers.txt", kArrays, 1, BLOODMOD_LASER_COUNT, LASER_RED_SMALL,
			  kNames, kNumNames, kFields, kNumFields);

} // namespace

const LaserRecipe* BloodModLaser_Get(int nType)
{
	return nType >= LASER_RED_SMALL && nType < LASER_RED_SMALL + BLOODMOD_LASER_COUNT ? &g_Lasers[nType - LASER_RED_SMALL] : 0;
}
const char* BloodModLaser_Name(int nType)						{ return g_Table.Name(nType); }
int BloodModLaser_Find(const char* pName)
{
	const int n = g_Table.Find(pName);
	return n >= LASER_RED_SMALL ? n : -1;
}
const char* BloodModLaser_Key(int nField)						{ return g_Table.Key(nField); }
std::string BloodModLaser_Value(int nType, int nField, bool bRetail)	{ return g_Table.Value(nType, nField, bRetail); }
bool BloodModLasers_Load(LT1Mod::FileSource &src, std::string &sError)	{ return g_Table.Load(src, sError); }
void BloodModLasers_Clear()										{ g_Table.Clear(); }
std::string BloodModLasers_Summary()							{ return g_Table.Summary(); }
std::string BloodModLasers_Reference()							{ return g_Table.Reference(); }
void BloodModLasers_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	g_Table.LoadAndReport(src, pPrint, pPrefix);
}
