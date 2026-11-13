// BloodModExplosions.h: Blood II's explosions, which a mod can add to, replace, and add new ones
// of.
//
// An explosion is what the client draws for an EXP_ id (ClientShellDLL/ExplosionFX.cpp): a recipe
// of pieces (models, sprites, particle rings, lights, fragments, waves, flames) built in code per
// detail level (ExplosionsDetail 2, 1, else). A mod ships Mod\Explosions.txt:
//
//     [NAPALM_PRIMARY_HIGH]                 // retail's, at high detail
//     Piece1 = "Sprite Sprite=Sprites\smokepuff3.spr Offset=15 Normal=1 Scale1=<3.25,3.25,0> Scale2=<4,4,0> Duration=10 Alpha=0.85 WaveForm=1 FadeType=1"
//
//     [SPARKLER]                            // a new explosion
//     Piece1 = "Light Offset=10 Color1=<1,1,0> Color2=<1,0.5,0> Duration=2 Radius1=200"
//
// A section is an explosion's EXP_ name without the prefix, with _HIGH, _MEDIUM or _LOW for one
// detail level, or none for all three. Its pieces are added after retail's; Replace = TRUE draws
// them instead of retail's. A name retail has not is a new explosion, numbered from
// BLOODMOD_EXP_FIRST_NEW in file order. Both modules load the file, so the server's id is the
// client's. A projectile, a weapon or a script names it as it names a retail one.
//
// A piece is one string: its kind (Model, Sprite, Ring, Light, Frag, Wave, Flame), then
// Field=value pairs, the create struct's members without their type letter (Scale1, Duration,
// Alpha, Particle, ...), and:
//     Offset=d          placed d units out along the explosion's normal
//     Normal=1          faces along the explosion's normal
//     NormalVector=<x,y,z>  faces that way instead
//     Count=n or a..b   made that many times, each with fresh random values
//     Pick=a..b         a whole number drawn per piece, put for %d in its
//                       paths (retail's stone chunks: gib%d.abc, 6..9)
// A value is a number, a..b (a random number between, drawn per piece), a vector <x,y,z> whose
// parts may be ranges, a path, or for SpawnType an EXP_ name and for TrailType an OBJFX_ name.
// Every field not given is 0, as the structs are. The client alone draws them; the server reads
// the names.

#ifndef __BLOODMODEXPLOSIONS_H__
#define __BLOODMODEXPLOSIONS_H__

#include <string>
#include <vector>

namespace LT1Mod { class FileSource; }

#define BLOODMOD_EXP_FIRST_NEW	100		// above every retail EXP_ id, the add-on's too
#define BLOODMOD_EXP_MAX_NEW	64

enum BloodModFxKind { BMX_MODEL, BMX_SPRITE, BMX_RING, BMX_LIGHT, BMX_FRAG, BMX_WAVE, BMX_FLAME, BMX_KINDS };

// Every field any kind has; BloodModExplosion_FieldName says which kinds.
enum BloodModFxField
{
	BMF_OFFSET, BMF_NORMAL, BMF_NORMALVECTOR, BMF_COUNT,
	BMF_SCALE1, BMF_SCALE2, BMF_ROTATIONS, BMF_DURATION, BMF_ALPHA, BMF_WAVEFORM, BMF_FADETYPE, BMF_RANDOMROT,
	BMF_MODEL, BMF_SKIN, BMF_SPRITE, BMF_ALIGN, BMF_COLOR, BMF_RADIUS, BMF_POSRADIUS, BMF_VELOCITY, BMF_GRAVITY,
	BMF_ROTATION, BMF_PARTICLES, BMF_DELAY, BMF_ROTATETYPE, BMF_PARTICLE, BMF_COLOR1, BMF_COLOR2, BMF_COLOR3,
	BMF_LIFETIMES, BMF_LIFEOFFSETS, BMF_FXTIMES, BMF_RAMPFLAGS, BMF_RADIUS1, BMF_RADIUS2, BMF_SCALE, BMF_ROTATEMAX,
	BMF_SPREAD, BMF_BOUNCEMOD, BMF_FADETIME, BMF_INITALPHA, BMF_RANDDIR, BMF_SPAWNEXP, BMF_SPAWNTYPE, BMF_TRAILTYPE,
	BMF_WAVE, BMF_PICK,
	BMF_FIELDS
};

// A value: a number or a range, a vector of them, a path, or a name's id.
struct BloodModFxValue
{
	bool		bSet;
	float		lo[3], hi[3];	// a number is lo[0] == hi[0]; a vector all three
	std::string	s;				// a path
	int			nId;			// an EXP_ or OBJFX_ id
};

struct BloodModFxPiece
{
	int				nKind;
	BloodModFxValue	v[BMF_FIELDS];
};

struct BloodModFxRecipe
{
	bool							bReplace;
	std::vector<BloodModFxPiece>	pieces;
};

// The mod's recipe for explosion nId at detail 0 (low), 1 (medium) or 2
// (high), or NULL: retail's alone.
const BloodModFxRecipe* BloodModExplosion_Recipe(int nId, int nDetail);

// An EXP_ id by name without the prefix, retail's or a mod's new one, or -1;
// an id's name, or NULL.
int BloodModExplosion_Find(const char* pName);
const char* BloodModExplosion_Name(int nId);
int BloodModExplosion_NewCount();

const char* BloodModExplosion_KindName(int nKind);
const char* BloodModExplosion_FieldName(int nField);
// One piece's string, parsed: false and sError on a bad one.
bool BloodModExplosion_ParsePiece(const char* pText, BloodModFxPiece& out, std::string& sError);
std::string BloodModExplosion_PieceText(const BloodModFxPiece& p);

bool BloodModExplosions_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModExplosions_Clear();
std::string BloodModExplosions_Summary();
std::string BloodModExplosions_Reference();
void BloodModExplosions_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODEXPLOSIONS_H__
