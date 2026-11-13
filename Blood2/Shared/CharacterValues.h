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

// A character's attributes and what it starts with, which retail wrote as code in three places:
//
//   nAttrib    CBloodClientShell::SetCharacterInfo: what the character menu, and so a new game,
//              gives (strength, speed, resistance, magic), each 1-5, as
//              CPlayerObj::ValidateAttributes clamps
//   nSwitch    CPlayerObj::TweakSaveValuesForCharacterChange: what an add-on level that switches
//              character gives (DoLevelChangeCharacterCheck, the _CC_ levels). 0 is nAttrib's.
//              Retail's two tables agree for the four Chosen, so they are 0 here. The switch
//              has no case for the add-on's four, which no level switches to, and gives them 1.
//   szWeapons, szItems, szAmmo
//              CPlayerObj::ResetPlayerInventory, which that switch runs: WEAP_ names without
//              the prefix, each ObtainWeapon(w, 0) in order; INV_ names, each AddItem(i, 100)
//              in order; and AMMO_ names, each with its count, set after every count is set to
//              0. The single-player respawn sets the ammo the same way.
struct CharacterStart
{
	int		nAttrib[4];
	int		nSwitch[4];
	char*	szWeapons;
	char*	szItems;
	char*	szAmmo;
	char*	szInitWeapons;	// the client's single-player init: weapon slots 1-9
	char*	szCheatWeapons;	// what the "kfa" cheat gives, WEAP_ names
};

extern CharacterStart gCharacterStart[BLOOD2_CHARACTER_IDS];

#endif // __CHARACTERVALUES_H__
