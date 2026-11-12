// The AI_Mgr members a mod's Mod\AI.txt sets (BloodModAI.h).
//
// BloodModAIValues holds them under AI_Mgr's own names and types, so AI_Mgr.cpp copies them in
// and out with BLOODMOD_AI_FIELDS and a member misnamed or mistyped here does not compile. The
// struct is written out by hand rather than generated from the list, so tests/Blood2Tables can
// check the list against it independently: every member some key's, and once.

#ifndef __BLOODMODAIFIELDS_H__
#define __BLOODMODAIFIELDS_H__

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
};

// X(key, member, kind): kind is FLOAT (0 or more), INT (0 or more) or STR (at most 31
// characters, as AI_Mgr::ReadProp copies a level's). Where a level property sets the same member,
// the key is that property's name.
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
	X(Proximity_Bombs,	m_fProxBombs,		FLOAT)

#endif // __BLOODMODAIFIELDS_H__
