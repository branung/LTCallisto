// BloodModObjectFX.h: Blood II's object particle effects, which a mod can change.
//
// An object effect is what the client draws for an OBJFX_ id (SFXMsgIds.h) sent with
// SFX_OBJECTFX_ID: one particle system (OBJECTPARTICLEFXCS) that CObjectGeneralFX::CreateObject
// (ClientShellDLL/ObjectFX.cpp) fills in code, per detail level (ObjectParticles 2 or more, 1,
// else). A mod ships Mod\ObjectFX.txt:
//
//     [FLAMING_2_HIGH]                      // retail's, at high detail
//     Particle = "SpriteTextures\flames1_09.dtx"
//
//     [SMOKING_1]                           // all three detail levels
//     Density  = 0.4
//     Alpha    = 0.05
//
// A section is an OBJFX_ name without the prefix, with _HIGH, _MEDIUM or _LOW for one detail
// level, or none for all three (not both for one level). Its keys replace those fields of retail's
// recipe and leave the rest; Replace = TRUE starts from an empty recipe instead, keeping only the
// object the effect follows. The keys are the create struct's members without their type letter:
//     Offset, Rotations, MinColor, MaxColor   vectors, <x, y, z>
//     RampUpTime, RampDownTime, Duration, AddDelay, Radius, Gravity,
//     PosRadius, Density, Alpha, MinVelocity, MaxVelocity, MinLifetime,
//     MaxLifetime                             numbers (Gravity and the
//                                             vectors may be negative)
//                                             A number may be a range
//                                             instead, "75..100", drawn each
//                                             time as retail's GetRandom is,
//                                             and a vector's parts too,
//                                             "<0..0.01, 0..0.015, 0>"
//     NumParticles                            a whole number, 0 or more
//     EmitType     LOCATION, RING, SPHERE, CYLINDER, FILLEDRING, FILLEDSPHERE,
//                  FILLEDCYLINDER, COMETTAIL or POWERRING
//     RampUpType, RampDownType   ALPHA, VELOCITY, NUMPARTICLES, ROTATION and
//                  POSRADIUS joined by |, "" for none
//     FXFlags      REMOVESTOPPED, RAMPDOWNSTOPPED, MOVINGSOURCE, MOVINGSTREAM,
//                  ROTATINGSOURCE, ALIGNROTATION, or SMOOTHSTOP, STREAMING,
//                  FREEROTATION, ALL, joined by |, "" for none
//     Particle     a sprite texture, "" for none
// The server's own flags and scale (an object's SFX_OBJECTFX message) still apply afterwards, as
// they apply to retail's. The client alone reads it.

#ifndef __BLOODMODOBJECTFX_H__
#define __BLOODMODOBJECTFX_H__

#include <string>

namespace LT1Mod { class FileSource; }

enum BloodModObjFXField
{
	BMO_OFFSET, BMO_RAMPUPTIME, BMO_RAMPDOWNTIME, BMO_DURATION, BMO_ADDDELAY, BMO_RADIUS, BMO_GRAVITY,
	BMO_ROTATIONS, BMO_POSRADIUS, BMO_EMITTYPE, BMO_NUMPARTICLES, BMO_DENSITY, BMO_ALPHA, BMO_MINCOLOR,
	BMO_MAXCOLOR, BMO_MINVELOCITY, BMO_MAXVELOCITY, BMO_MINLIFETIME, BMO_MAXLIFETIME, BMO_RAMPUPTYPE,
	BMO_RAMPDOWNTYPE, BMO_FXFLAGS, BMO_PARTICLE,
	BMO_FIELDS
};

// One section's fields: bSet says which; a number is lo[f][0]..hi[f][0], a
// vector all three, each part drawn between its two as retail's GetRandom
// draws (the same where it is no range); a name or a flag word n[f], the
// particle sParticle.
struct BloodModObjFXRecipe
{
	bool			bReplace;
	bool			bSet[BMO_FIELDS];
	float			lo[BMO_FIELDS][3];
	float			hi[BMO_FIELDS][3];
	unsigned int	n[BMO_FIELDS];
	std::string		sParticle;
};

// The mod's recipe for OBJFX id nId at detail 0 (low), 1 (medium) or 2
// (high), or NULL: retail's alone.
const BloodModObjFXRecipe* BloodModObjectFX_Recipe(int nId, int nDetail);

const char* BloodModObjectFX_FieldName(int nField);		// "NumParticles", or NULL
// One field of a recipe as the file writes it ("0.4", "<20, 0, 0>",
// "\"RAMPALPHA\"", ...).
std::string BloodModObjectFX_Value(const BloodModObjFXRecipe& r, int nField);

bool BloodModObjectFX_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModObjectFX_Clear();
// One line per section and level: "FLAMING_2_HIGH: Particle "...", ...".
std::string BloodModObjectFX_Summary();
std::string BloodModObjectFX_Reference();
void BloodModObjectFX_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODOBJECTFX_H__
