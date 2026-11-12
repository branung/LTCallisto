#include "server_de.h"
#include "SharedDefs.h"
#include "CharacterValues.h"
#include "BloodModCharacters.h"
#include "BloodModTable.h"

#include <stddef.h>
#include <string>

using namespace BloodMod;

namespace
{

// The CHARACTER_ names, by id (SharedDefs.h). Every id below MAX_CHARACTERS has exactly one.
#define C(x) { CHARACTER_##x, #x }
const Entry kCharacterNames[] =
{
	C(CALEB), C(OPHELIA), C(ISHMAEL), C(GABREILLA),
#ifdef _ADD_ON
	C(M_CULTIST), C(F_CULTIST), C(SOULDRUDGE), C(PROPHET),
#endif
};
#undef C
const int kNumNames = (int)(sizeof(kCharacterNames) / sizeof(kCharacterNames[0]));

// Array 0 is gCharacterValues, array 1 is gCharacterSkins (a row of names per character),
// array 2 is gCharacterPictures (the menu's).
#define F(key, member, kind) { key, 0, offsetof(CharacterValues, member), kind, 0 }
#define S(key, skin) { key, 1, (skin) * sizeof(char*), K_STRING, 0 }
#define M(key, pic) { key, 2, (pic) * sizeof(char*), K_STRNULL, 0 }
const Field kFields[] =
{
	F("Name",				szName,					K_STRING),
	F("Model",				szModelName,			K_STRING),
	F("Height",				fHeightStanding,		K_FLOAT),
	F("EyeLevel",			fEyeLevelStanding,		K_FLOAT),
	F("EyeLevelCrouching",	fEyeLevelCrouching,		K_FLOAT),
	F("EyeLevelDead",		fEyeLevelDead,			K_FLOAT),
	S("Skin",				MULTIPLAY_SKIN_NORMAL),
	S("SkinRed",			MULTIPLAY_SKIN_RED),
	S("SkinBlue",			MULTIPLAY_SKIN_BLUE),
	S("SkinGreen",			MULTIPLAY_SKIN_GREEN),
	S("SkinYellow",			MULTIPLAY_SKIN_YELLOW),
	S("SkinTeamBlue",		5),		// CalTEAM_B.dtx; SharedDefs.h names neither team slot
	S("SkinTeamRed",		6),		// CalTEAM_R.dtx
	M("Picture",			MULTIPLAY_SKIN_NORMAL),	// the character menu's; "" is none
	M("PictureRed",			MULTIPLAY_SKIN_RED),
	M("PictureBlue",		MULTIPLAY_SKIN_BLUE),
	M("PictureGreen",		MULTIPLAY_SKIN_GREEN),
	M("PictureYellow",		MULTIPLAY_SKIN_YELLOW),
};
#undef F
#undef S
#undef M
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const Array kArrays[] =
{
	{ gCharacterValues, sizeof(gCharacterValues[0]) },
	{ gCharacterSkins, sizeof(gCharacterSkins[0]) },
	{ gCharacterPictures, sizeof(gCharacterPictures[0]) },
};

Table g_Table("Mod\\Characters.txt", "a Blood 2 character", "the CHARACTER_ names without the prefix",
			  "Blood2/ModReference/Characters.txt", kArrays, 3, MAX_CHARACTERS, CHARACTER_CALEB,
			  kCharacterNames, kNumNames, kFields, kNumFields,
			  BLOOD2_CHARACTER_IDS, "Base", "character");

} // namespace

const char* BloodModCharacter_Name(int nId)						{ return g_Table.Name(nId); }
int BloodModCharacter_Count()									{ return g_Table.Total(); }
int BloodModCharacter_Base(int nId)								{ return g_Table.Base(nId); }
int BloodModCharacter_Retail(int nId)							{ return g_Table.Retail(nId); }
int BloodModCharacter_Find(const char* pName)					{ return g_Table.Find(pName); }
const char* BloodModCharacter_Key(int nField)					{ return g_Table.Key(nField); }
std::string BloodModCharacter_Value(int nId, int nField, bool bRetail)	{ return g_Table.Value(nId, nField, bRetail); }
void BloodModCharacters_Clear()									{ g_Table.Clear(); }
bool BloodModCharacters_Load(LT1Mod::FileSource &src, std::string &sError)	{ return g_Table.Load(src, sError); }
std::string BloodModCharacters_Summary()						{ return g_Table.Summary(); }
std::string BloodModCharacters_Reference()						{ return g_Table.Reference(); }

void BloodModCharacters_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	g_Table.LoadAndReport(src, pPrint, pPrefix);
}
