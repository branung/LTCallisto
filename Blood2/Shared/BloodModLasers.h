// BloodModLasers.h: Blood II's laser beams' looks, which a mod can change.
//
// A laser beam is what the client draws for an SFX_LASERBEAM_ID message
// (ClientShellDLL/LaserBeamFX.cpp): a model stretched from one point to another, which grows from
// MinScale to MaxScale over ScaleTime and fades over FadeTime, gone after Duration, at Alpha, with
// a skin per color. CLaserBeamFX::SetupBeam wrote it as two switches over the fourteen LASER_ ids
// (SFXMsgIds.h). A mod ships Mod\Lasers.txt:
//
//     [YELLOW_SMALL]
//     MinScale = 0.2
//     Duration = 0.015625
//
// A section is a LASER_ name without the prefix; the keys are MinScale, MaxScale, Duration,
// ScaleTime, FadeTime, Alpha, WaveForm, Model and Skin, retail's values in
// Blood2/ModReference/Lasers.txt (testBlood2Tables --dump lasers). The client alone draws them.
// The server names a laser for a weapon's tracer (Mod\WeaponClasses.txt's Tracer), which is why
// both modules have the names.

#ifndef __BLOODMODLASERS_H__
#define __BLOODMODLASERS_H__

#include <string>

namespace LT1Mod { class FileSource; }

#define BLOODMOD_LASER_COUNT	14		// LASER_RED_SMALL .. LASER_PURPLE_LARGE

struct LaserRecipe
{
	float			fMinScale, fMaxScale, fDuration, fScaleTime, fFadeTime, fAlpha;
	unsigned int	bWaveForm;
	char*			szModel;
	char*			szSkin;
};

// Laser nType's recipe, the mod's or retail's; NULL for an id no laser has.
const LaserRecipe* BloodModLaser_Get(int nType);

const char* BloodModLaser_Name(int nType);		// "YELLOW_SMALL", or NULL
int BloodModLaser_Find(const char* pName);		// a LASER_ id, or -1
const char* BloodModLaser_Key(int nField);
std::string BloodModLaser_Value(int nType, int nField, bool bRetail);

bool BloodModLasers_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModLasers_Clear();
std::string BloodModLasers_Summary();
std::string BloodModLasers_Reference();
void BloodModLasers_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODLASERS_H__
