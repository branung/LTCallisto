// The AI_Mgr members a mod's Mod\AI.txt sets (BloodModAI.h).
//
// BloodModAIValues holds them under AI_Mgr's own names and types, so AI_Mgr.cpp copies them in
// and out with BLOODMOD_AI_FIELDS and a member misnamed or mistyped here does not compile. The
// struct is written out by hand rather than generated from the list, so tests/Blood2Tables can
// check the list against it independently: every member some key's, and once.

#ifndef __BLOODMODAIFIELDS_H__
#define __BLOODMODAIFIELDS_H__

#include "server_de.h"		// DDWORD, DBOOL: the members' own types

// AI.txt's Skin key, at most this many characters: LT1's MAX_CS_FILENAME_LEN
// (100) less the terminator, so a mod's skin fits the create struct and the
// filename buffers either engine sizes from it.
#define BLOODMOD_AI_MAX_SKIN	99

// The damage types the death table keys name (DeathSkin<Name>, DeathFX<Name>), in this order.
// Each is a DAMAGE_TYPE_ (SharedDefs.h).
#define BLOODMOD_DEATH_KINDS	7
#define BLOODMOD_DEATH_KIND_NAMES(X) \
	X(Normal) X(Fire) X(Acid) X(Electric) X(Explode) X(Singularity) X(Melee)

struct BloodModAIValues
{
	float	m_fHearingDist;
	float	m_fSensingDist;
	float	m_fSmellingDist;
	float	m_fSeeingDist;
	float	m_fWalkSpeed;
	float	m_fRunSpeed;
	float	m_fJumpSpeed;
	float	m_fRollSpeed;
	float	m_fAIMass;
	float	m_fAIHitPoints;
	float	m_fAIRandomHP;
	float	m_fAIArmorPoints;
	int		m_nAIStrength;
	char	m_szAIState[32];
	char	m_szAIWeapon[5][32];
	float	m_fAIBullets;
	float	m_fAIBMG;
	float	m_fAIShells;
	float	m_fAIGrenades;
	float	m_fAIRockets;
	float	m_fAIFlares;
	float	m_fAICells;
	float	m_fAICharges;
	float	m_fAIFuel;
	float	m_fProxBombs;
	DDWORD	m_dwFlags;
	DBOOL	m_bCabal;
	// Behavior: members added for a mod, not the 1998 code's; AI_Mgr.h says what reads each
	DBOOL	m_bModBestWeapon;
	DBOOL	m_bModChaseRun;
	DBOOL	m_bModChaseNear;
	char	m_szModAttackWeapon[32];
	char	m_szModNoAltFire[32];
	float	m_fModFadeMax;
	float	m_fModFadeMin;
	int		m_nModLoopVolume;
	float	m_fModSpawnCooldown;
	float	m_fModSpawnInterval;
	DBOOL	m_bModSpawnAlways;
	DBOOL	m_bModScript;
	// The death table, by BLOODMOD_DEATH_KINDS
	DBOOL	m_bModRobe;
	char	m_szModDeathSkin[BLOODMOD_DEATH_KINDS][BLOODMOD_AI_MAX_SKIN + 1];
	char	m_szModDeathFX[BLOODMOD_DEATH_KINDS][32];
	DBOOL	m_bModDeathAnimRandom;
	char	m_szModDropOnDeath[32];
	char	m_szModDropDivine[32];
	int		m_nModCorpseLimbLoss;
	int		m_nModGibLimbs;
	int		m_nModGibLimb[5];
};

// AI.txt's Flags key names AI_Mgr's m_dwFlags bits, AI_Mgr.h's FLAG_ names without the prefix
#define BLOODMOD_AI_FLAGS(X) \
	X(CRAWL,			0x00000001) \
	X(JUMP,				0x00000002) \
	X(LIMP,				0x00000004) \
	X(DODGE,			0x00000008) \
	X(LIMBLOSS,			0x00000010) \
	X(ALWAYSRECOIL,		0x00000020) \
	X(ALWAYSGIB,		0x00000040) \
	X(CANASSIST,		0x00000080) \
	X(ALWAYSCLEAR,		0x00000100) \
	X(NOAMMOCOLLECT,	0x00000200) \
	X(NIGHTMAREDEATH,	0x00000400) \
	X(NEVERGIB,			0x00000800)

// X(key, member, kind): kind is FLOAT (0 or more), INT (0 or more), STR (at most 31
// characters, as AI_Mgr::ReadProp copies a level's), PATH (a file, at most BLOODMOD_AI_MAX_SKIN
// characters), OBJFX ("" or an SFXMsgIds.h OBJFX_ name without the prefix), MODE (0 never, 1
// always, 2 the game's own rule), BOOL (TRUE or FALSE) or FLAGS (BLOODMOD_AI_FLAGS names joined by
// |, "" for none). Where a level property sets the same member, the key is that
// property's name.
#define BLOODMOD_AI_FIELDS(X) \
	X(HearingDist,		m_fHearingDist,		FLOAT) \
	X(SensingDist,		m_fSensingDist,		FLOAT) \
	X(SmellingDist,		m_fSmellingDist,	FLOAT) \
	X(SeeingDist,		m_fSeeingDist,		FLOAT) \
	X(WalkSpeed,		m_fWalkSpeed,		FLOAT) \
	X(RunSpeed,			m_fRunSpeed,		FLOAT) \
	X(JumpSpeed,		m_fJumpSpeed,		FLOAT) \
	X(RollSpeed,		m_fRollSpeed,		FLOAT) \
	X(Mass,				m_fAIMass,			FLOAT) \
	X(HitPoints,		m_fAIHitPoints,		FLOAT) \
	X(RandomHitPoints,	m_fAIRandomHP,		FLOAT) \
	X(ArmorPoints,		m_fAIArmorPoints,	FLOAT) \
	X(Strength,			m_nAIStrength,		INT) \
	X(AIState,			m_szAIState,		STR) \
	X(AIWeapon1,		m_szAIWeapon[0],	STR) \
	X(AIWeapon2,		m_szAIWeapon[1],	STR) \
	X(AIWeapon3,		m_szAIWeapon[2],	STR) \
	X(AIWeapon4,		m_szAIWeapon[3],	STR) \
	X(AIWeapon5,		m_szAIWeapon[4],	STR) \
	X(Bullets,			m_fAIBullets,		FLOAT) \
	X(BMG,				m_fAIBMG,			FLOAT) \
	X(Shells,			m_fAIShells,		FLOAT) \
	X(Grenades,			m_fAIGrenades,		FLOAT) \
	X(Rockets,			m_fAIRockets,		FLOAT) \
	X(Flares,			m_fAIFlares,		FLOAT) \
	X(Cells,			m_fAICells,			FLOAT) \
	X(Charges,			m_fAICharges,		FLOAT) \
	X(Fuel,				m_fAIFuel,			FLOAT) \
	X(Proximity_Bombs,	m_fProxBombs,		FLOAT) \
	X(Flags,			m_dwFlags,			FLAGS) \
	X(Cabal,			m_bCabal,			BOOL) \
	X(BestWeapon,		m_bModBestWeapon,	BOOL) \
	X(ChaseRun,			m_bModChaseRun,		BOOL) \
	X(ChaseNear,		m_bModChaseNear,	BOOL) \
	X(AttackWeapon,		m_szModAttackWeapon, STR) \
	X(NoAltFire,		m_szModNoAltFire,	STR) \
	X(FadeMax,			m_fModFadeMax,		FLOAT) \
	X(FadeMin,			m_fModFadeMin,		FLOAT) \
	X(LoopVolume,		m_nModLoopVolume,	INT) \
	X(SpawnCooldown,	m_fModSpawnCooldown, FLOAT) \
	X(SpawnInterval,	m_fModSpawnInterval, FLOAT) \
	X(SpawnAlways,		m_bModSpawnAlways,	BOOL) \
	X(Script,			m_bModScript,		BOOL) \
	X(Robe,				m_bModRobe,			BOOL) \
	X(DeathSkinNormal,	m_szModDeathSkin[0], PATH) \
	X(DeathSkinFire,	m_szModDeathSkin[1], PATH) \
	X(DeathSkinAcid,	m_szModDeathSkin[2], PATH) \
	X(DeathSkinElectric, m_szModDeathSkin[3], PATH) \
	X(DeathSkinExplode,	m_szModDeathSkin[4], PATH) \
	X(DeathSkinSingularity, m_szModDeathSkin[5], PATH) \
	X(DeathSkinMelee,	m_szModDeathSkin[6], PATH) \
	X(DeathFXNormal,	m_szModDeathFX[0],	OBJFX) \
	X(DeathFXFire,		m_szModDeathFX[1],	OBJFX) \
	X(DeathFXAcid,		m_szModDeathFX[2],	OBJFX) \
	X(DeathFXElectric,	m_szModDeathFX[3],	OBJFX) \
	X(DeathFXExplode,	m_szModDeathFX[4],	OBJFX) \
	X(DeathFXSingularity, m_szModDeathFX[5], OBJFX) \
	X(DeathFXMelee,		m_szModDeathFX[6],	OBJFX) \
	X(DeathAnimRandom,	m_bModDeathAnimRandom, BOOL) \
	X(DropOnDeath,		m_szModDropOnDeath,	STR) \
	X(DropOnDeathDivine, m_szModDropDivine,	STR) \
	X(CorpseLimbLoss,	m_nModCorpseLimbLoss, MODE) \
	X(GibLimbs,			m_nModGibLimbs,		MODE) \
	X(GibHead,			m_nModGibLimb[0],	INT) \
	X(GibRightArm,		m_nModGibLimb[1],	INT) \
	X(GibLeftArm,		m_nModGibLimb[2],	INT) \
	X(GibRightLeg,		m_nModGibLimb[3],	INT) \
	X(GibLeftLeg,		m_nModGibLimb[4],	INT)

// The BLOODMOD_AI_FIELDS members AI_Mgr::Save does not write, which a restore takes from the mod
// again (ModAI_ApplyRestored, ModAI_ApplyUnsaved). m_bCabal is separate: it is needed before the
// restored object is filed on its list.
#define BLOODMOD_AI_UNSAVED(X) \
	X(m_bModBestWeapon) \
	X(m_bModChaseRun) \
	X(m_bModChaseNear) \
	X(m_szModAttackWeapon) \
	X(m_szModNoAltFire) \
	X(m_fModFadeMax) \
	X(m_fModFadeMin) \
	X(m_nModLoopVolume) \
	X(m_fModSpawnCooldown) \
	X(m_fModSpawnInterval) \
	X(m_bModSpawnAlways) \
	X(m_bModScript) \
	X(m_bModRobe) \
	X(m_szModDeathSkin) \
	X(m_szModDeathFX) \
	X(m_bModDeathAnimRandom) \
	X(m_szModDropOnDeath) \
	X(m_szModDropDivine) \
	X(m_nModCorpseLimbLoss) \
	X(m_nModGibLimbs) \
	X(m_nModGibLimb)

#endif // __BLOODMODAIFIELDS_H__
