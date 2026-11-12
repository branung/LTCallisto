// The player characters' table (CharacterValues.cpp), out of PlayerObj.h unchanged. CPlayerObj
// reads it by m_nCharacter (CHARACTER_CALEB ..), and the skins by m_nSkin too
// (MULTIPLAY_SKIN_NORMAL .. MULTIPLAY_SKIN_MAX, the last two the team skins).

#ifndef __CHARACTERVALUES_H__
#define __CHARACTERVALUES_H__

#include "SharedDefs.h"

struct CharacterValues
{
	char*	szName;
	char*	szModelName;
	char*	szSkinName;
	DFLOAT	fHeightStanding;
	DFLOAT	fHeightCrouching;
	DFLOAT	fHeightDead;
	DFLOAT	fEyeLevelStanding;
	DFLOAT	fEyeLevelCrouching;
	DFLOAT	fEyeLevelDead;
};

// Room for a mod's new characters after retail's, which Mod\Characters.txt adds with a Base
// (BloodModCharacters.h). The elements past retail's are zero until a mod adds one. An id is a
// byte on the wire and in a .b2c file's dwCharacter, so this is far inside both.
#define BLOOD2_CHARACTER_IDS	32

extern CharacterValues gCharacterValues[BLOOD2_CHARACTER_IDS];
extern char *gCharacterSkins[BLOOD2_CHARACTER_IDS][MULTIPLAY_SKIN_MAX+1];

// The character menu's pictures, one per MULTIPLAY_SKIN_NORMAL .. MULTIPLAY_SKIN_YELLOW, NULL
// where the menu has no entry (CharacterValues.cpp)
#define BLOOD2_CHARACTER_PICTURES	5
extern char *gCharacterPictures[BLOOD2_CHARACTER_IDS][BLOOD2_CHARACTER_PICTURES];

#endif // __CHARACTERVALUES_H__
