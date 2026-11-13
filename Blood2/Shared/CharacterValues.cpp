// The player characters' table, split out of PlayerObj.cpp so tests can build it alone.
// BloodModCharacters.cpp writes a mod's values into it.

#include "server_de.h"
#include "SharedDefs.h"
#include "CharacterValues.h"

CharacterValues gCharacterValues[BLOOD2_CHARACTER_IDS] = {
	// Caleb
	{
		"Caleb",
		"Models\\Characters\\Caleb.abc",
		"Skins\\Characters\\Caleb1.dtx",
		82.0f,	// Standing height
		26.0f,	// Crouching height
		16.0f,	// Dead Height
		36.0f,	// Standing eyelevel
		12.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	},
	// Ophelia
	{
		"Ophelia",
		"Models\\Characters\\Ophelia.abc",
		"Skins\\Characters\\Ophelia.dtx",
		76.0f,	// Standing height
		26.0f,	// Crouching height
		16.0f,	// Dead Height
		32.0f,	// Standing eyelevel
		12.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	},
	// Ishmael
	{
		"Ishmael",
		"Models\\Characters\\Ishmael.abc",
		"Skins\\Characters\\Ishmael.dtx",
		82.0f,	// Standing height
		26.0f,	// Crouching height
		16.0f,	// Dead Height
		36.0f,	// Standing eyelevel
		12.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	},
	// Gabriella
	{
		"Gabriella",
		"Models\\Characters\\Gabriella.abc",
		"Skins\\Characters\\Gabriella.dtx",
		88.0f,	// Standing height
		26.0f,	// Crouching height
		16.0f,	// Dead Height
		42.0f,	// Standing eyelevel
		12.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	}

#ifdef _ADDON
	,
	// Male Cultist
	{
		"M_Cultist",
		"Models_ao\\Characters_ao\\m_cultist1_bb.abc",
		"Skins\\Enemies\\m_cultist1.dtx",
		82.0f,	// Standing height
		26.0f,	// Crouching height
		16.0f,	// Dead Height
		36.0f,	// Standing eyelevel
		12.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	},
	// Female Cultist
	{
		"F_Cultist",
		"Models_ao\\Characters_ao\\f_cultist1_bb.abc",
		"Skins\\Enemies\\f_cultist1.dtx",
		76.0f,	// Standing height
		26.0f,	// Crouching height
		16.0f,	// Dead Height
		32.0f,	// Standing eyelevel
		12.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	},
	// Soul Drudge
	{
		"SoulDrudge",
		"Models_ao\\Characters_ao\\SoulDrudge_bb.abc",
		"Skins\\Enemies\\SoulDrudge.dtx",
		85.0f,	// Standing height
		29.0f,	// Crouching height
		16.0f,	// Dead Height
		38.0f,	// Standing eyelevel
		14.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	},
	// Prophet
	{
		"Prophet",
		"Models_ao\\Characters_ao\\Prophet_bb.abc",
		"Skins\\Enemies\\Prophet.dtx",
		81.0f,	// Standing height
		25.0f,	// Crouching height
		16.0f,	// Dead Height
		35.0f,	// Standing eyelevel
		11.0f,	// Crouching eyelevel
		0.0f	// dead eyelevel
	}
#endif

};

char *gCharacterSkins[BLOOD2_CHARACTER_IDS][MULTIPLAY_SKIN_MAX+1] =
{
	{
		"Skins\\Characters\\Caleb1.dtx",
		"Skins\\Characters\\CalMULTI_R.dtx",
		"Skins\\Characters\\CalMULTI_B.dtx",
		"Skins\\Characters\\CalMULTI_G.dtx",
		"Skins\\Characters\\CalMULTI_Y.dtx",
		"Skins\\Characters\\CalTEAM_B.dtx",
		"Skins\\Characters\\CalTEAM_R.dtx",
	},
	{
		"Skins\\Characters\\Ophelia.dtx",
		"Skins\\Characters\\OphMULTI_R.dtx",
		"Skins\\Characters\\OphMULTI_B.dtx",
		"Skins\\Characters\\OphMULTI_G.dtx",
		"Skins\\Characters\\OphMULTI_Y.dtx",
		"Skins\\Characters\\OphTEAM_B.dtx",
		"Skins\\Characters\\OphTEAM_R.dtx",
	},
	{
		"Skins\\Characters\\Ishmael.dtx",
		"Skins\\Characters\\IshMULTI_R.dtx",
		"Skins\\Characters\\IshMULTI_B.dtx",
		"Skins\\Characters\\IshMULTI_G.dtx",
		"Skins\\Characters\\IshMULTI_Y.dtx",
		"Skins\\Characters\\IshTEAM_B.dtx",
		"Skins\\Characters\\IshTEAM_R.dtx",
	},
	{
		"Skins\\Characters\\Gabriella.dtx",
		"Skins\\Characters\\GabMULTI_R.dtx",
		"Skins\\Characters\\GabMULTI_B.dtx",
		"Skins\\Characters\\GabMULTI_G.dtx",
		"Skins\\Characters\\GabMULTI_Y.dtx",
		"Skins\\Characters\\GabTEAM_B.dtx",
		"Skins\\Characters\\GabTEAM_R.dtx",
	},

#ifdef _ADDON
	{
		"Skins\\Enemies\\m_cultist1.dtx",	// male cultist
		"Skins_ao\\Enemies_ao\\m_Cultist5.dtx",
		"Skins_ao\\Enemies_ao\\m_Cultist4.dtx",
		"Skins_ao\\Enemies_ao\\m_Cultist6.dtx",
		"Skins\\Enemies\\m_cultist3.dtx",
		"Skins_ao\\Enemies_ao\\mc_team_b.dtx",
		"Skins_ao\\Enemies_ao\\mc_team_r.dtx",
	},
	{
		"Skins\\Enemies\\f_cultist1.dtx",	// female cultist
		"Skins\\Enemies\\f_cultist1.dtx",
		"Skins\\Enemies\\f_cultist1.dtx",
		"Skins\\Enemies\\f_cultist1.dtx",
		"Skins\\Enemies\\f_cultist1.dtx",
		"Skins_ao\\Enemies_ao\\fc_team_b.dtx",
		"Skins_ao\\Enemies_ao\\fc_team_r.dtx",
	},
	{
		"Skins\\Enemies\\souldrudge.dtx",	// soul drudge
		"Skins\\Enemies\\souldrudge.dtx",
		"Skins\\Enemies\\souldrudge.dtx",
		"Skins\\Enemies\\souldrudge.dtx",
		"Skins\\Enemies\\souldrudge.dtx",
		"Skins_ao\\Enemies_ao\\sd_team_b.dtx",
		"Skins_ao\\Enemies_ao\\sd_team_r.dtx",
	},
	{
		"Skins\\Enemies\\prophet.dtx",		// prophet
		"Skins\\Enemies\\prophet.dtx",
		"Skins\\Enemies\\prophet.dtx",
		"Skins\\Enemies\\prophet.dtx",
		"Skins\\Enemies\\prophet.dtx",
		"Skins_ao\\Enemies_ao\\p_team_b.dtx",
		"Skins_ao\\Enemies_ao\\p_team_r.dtx",
	},
#endif

};

// The character menu's pictures, by MULTIPLAY_SKIN_NORMAL to MULTIPLAY_SKIN_YELLOW.
// Entries are listed per character in id order, and NULL means no entry for that color.
// Retail prefixed the picture directory to every name without "_AO"; it is written out here.
#define PIC(x) "interface\\charscreen\\characterpics\\" x
char *gCharacterPictures[BLOOD2_CHARACTER_IDS][BLOOD2_CHARACTER_PICTURES] =
{
	{ PIC("caleb_single.pcx"), PIC("caleb_red.pcx"), PIC("caleb_blue.pcx"), PIC("caleb_green.pcx"), PIC("caleb_yellow.pcx") },
	{ PIC("ophelia_single.pcx"), PIC("ophelia_red.pcx"), PIC("ophelia_blue.pcx"), PIC("ophelia_green.pcx"), PIC("ophelia_yellow.pcx") },
	{ PIC("ishmael_single.pcx"), PIC("ishmael_red.pcx"), PIC("ishmael_blue.pcx"), PIC("ishmael_green.pcx"), PIC("ishmael_yellow.pcx") },
	{ PIC("gabby_single.pcx"), PIC("gabby_red.pcx"), PIC("gabby_blue.pcx"), PIC("gabby_green.pcx"), PIC("gabby_yellow.pcx") },

#ifdef _ADD_ON
	{ "interface_AO\\m_cultist1.pcx", "interface_AO\\clown2.pcx", "interface_AO\\clown1.pcx", "interface_AO\\clown3.pcx",
	  "interface_AO\\m_cultist3.pcx" },
	{ "interface_AO\\f_cultist1.pcx" },
	{ "interface_AO\\souldrudge1.pcx" },
	{ "interface_AO\\prophet.pcx" },
#endif
};
#undef PIC

// A character's attributes and start inventory (CharacterValues.h): retail's SetCharacterInfo,
// TweakSaveValuesForCharacterChange and ResetPlayerInventory, as a table.
// CPlayerObj's CHEAT_KFA case, every character's alike in retail
#define CHEAT_KFA_WEAPONS	(char*)"BERETTA SUBMACHINEGUN SHOTGUN ASSAULTRIFLE LIFELEECH TESLACANNON ORB VOODOO DEATHRAY " \
							"MINIGUN NAPALMCANNON SNIPERRIFLE"
CharacterStart gCharacterStart[BLOOD2_CHARACTER_IDS] =
{
	{ { 5, 3, 3, 1 }, { 0, 0, 0, 0 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// Caleb
	{ { 2, 5, 1, 4 }, { 0, 0, 0, 0 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// Ophelia
	{ { 1, 3, 3, 5 }, { 0, 0, 0, 0 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// Ishmael
	{ { 5, 1, 5, 1 }, { 0, 0, 0, 0 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// Gabriella

#ifdef _ADD_ON
	{ { 5, 3, 3, 1 }, { 1, 1, 1, 1 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// male cultist
	{ { 2, 5, 1, 4 }, { 1, 1, 1, 1 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// female cultist
	{ { 1, 3, 3, 5 }, { 1, 1, 1, 1 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// soul drudge
	{ { 5, 1, 5, 1 }, { 1, 1, 1, 1 }, "MELEE BERETTA", "", "BULLET 50", "BERETTA", CHEAT_KFA_WEAPONS },	// prophet
#endif
};
