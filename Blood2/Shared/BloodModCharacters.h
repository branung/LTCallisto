// Blood II's player characters' table, overridable by a mod's Mod\Characters.txt:
//
//     [CALEB]
//     Model = "Models\Characters\Caleb2.abc"
//     EyeLevel = 40.0
//
//     [OPHELIA]
//     SkinRed = "Skins\Characters\OphMULTI_R2.dtx"
//
// gCharacterValues, gCharacterSkins and gCharacterPictures (CharacterValues.cpp) then hold the
// mod's value for that field of that character. As with the weapons, the tables are written in
// place once, when a module loads the mod, from a retail copy, and no consumer changes. The server
// reads the models, skins and eye levels (the client is sent the model and skin names); the
// client reads the menu's pictures.
//
// A section is a character's CHARACTER_ name without the prefix (SharedDefs.h: CALEB, OPHELIA,
// ISHMAEL, GABREILLA, the define's spelling, and with the Nightmare Levels M_CULTIST, F_CULTIST,
// SOULDRUDGE, PROPHET), matched without case. The keys, in BloodModCharacters.cpp's kFields and
// listed with their retail values in Blood2/ModReference/Characters.txt
// (`testBlood2Tables --dump characters`):
//
//     Name              the sound directory, sounds\chosen\<Name> (SetCharacter)
//     Model             the model (SetCharacter)
//     Height            the standing height, which sets only the first dims (InitialUpdate); the
//                       animations' own dims replace them
//     EyeLevel, EyeLevelCrouching, EyeLevelDead
//     Skin, SkinRed, SkinBlue, SkinGreen, SkinYellow, SkinTeamBlue, SkinTeamRed
//                       gCharacterSkins, by MULTIPLAY_SKIN_ (the last two the team skins)
//     Picture, PictureRed, PictureBlue, PictureGreen, PictureYellow
//                       the character menu's picture for that color, a .pcx; "" is no entry
//
// The three fields nothing reads (szSkinName, fHeightCrouching, fHeightDead) are not keys, so a
// mod that sets them is told, not ignored.
//
// A section naming no character adds one, built on its Base (a retail character or a new one
// earlier in the file) and numbered after retail's (4, or 8 with the Nightmare Levels) in file
// order, up to BLOOD2_CHARACTER_IDS - 1:
//
//     [SCARLET]
//     Base    = "CALEB"
//     Skin    = "Skins\Characters\CalebRed.dtx"
//
// It starts as its Base as this mod leaves it. What the game decides by a CHARACTER_ id rather
// than by the table it decides by BloodModCharacter_Retail, so a new character is its Base there.

#ifndef __BLOODMODCHARACTERS_H__
#define __BLOODMODCHARACTERS_H__

#include <string>

namespace LT1Mod { class FileSource; }

// The section name of character nId (CHARACTER_CALEB .. Count() - 1, a mod's new ones too), or
// NULL; an id by section name, without case, or -1
const char* BloodModCharacter_Name(int nId);
int BloodModCharacter_Find(const char* pName);
// Every character, retail's and the mod's: ids CHARACTER_CALEB .. Count() - 1
int BloodModCharacter_Count();
// A new character's Base, or -1 for any other id; and the retail character at the end of its
// chain, or any other id itself, so it stands wherever retail code tests a CHARACTER_ id
int BloodModCharacter_Base(int nId);
int BloodModCharacter_Retail(int nId);

// The keys, in table order; NULL past the last. A value as Characters.txt writes it, the current
// table's or retail's.
const char* BloodModCharacter_Key(int nField);
std::string BloodModCharacter_Value(int nId, int nField, bool bRetail = false);

// As BloodModWeapons_* (BloodModWeapons.h), for Mod\Characters.txt
bool BloodModCharacters_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModCharacters_Clear();
std::string BloodModCharacters_Summary();
std::string BloodModCharacters_Reference();
std::string BloodModCharacters_Current();	// the same, for the table as it now is
void BloodModCharacters_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODCHARACTERS_H__
