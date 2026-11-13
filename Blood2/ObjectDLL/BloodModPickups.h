// BloodModPickups.h: what Blood II's item pickups give, which a mod can change.
//
// An item pickup is a class a level places (TheEyePU, MedKitPU, ...), and its constructor named
// the item it gives, the model it shows (Models\Powerups\<Model>.abc), the name it says, and the
// string that name is (ClientRes.h's IDS_ITEM_). A mod ships Mod\Pickups.txt:
//
//     [TheEyePU]                // every Seeing Eye in a level is a medkit
//     Item   = "MEDKIT"
//     Model  = "Medkit_pu"
//     Name   = "Medkit"
//     NameID = "ITEM_MEDKIT"
//
// A section is the class's name; the keys are Item (an INV_ name without the prefix), Model, Name
// and NameID (an IDS_ITEM_ name without IDS_). The five charged items (binoculars, eye,
// flashlight, medkit, goggles) give one of those five, the three bombs one of the bombs. Retail's
// values are in Blood2/ModReference/Pickups.txt. The server alone loads it.

#ifndef __BLOODMODPICKUPS_H__
#define __BLOODMODPICKUPS_H__

#include <string>

namespace LT1Mod { class FileSource; }

enum BloodModPickupRow
{
	BMPU_BINOCULARS, BMPU_THEEYE, BMPU_FLASHLIGHT, BMPU_MEDKIT, BMPU_NIGHTGOGGLES,	// ItemPickupCharged
	BMPU_PROXIMITIES, BMPU_REMOTES, BMPU_TIMEBOMB,									// ItemPickupWeapon
	BMPU_COUNT
};

struct BloodModItemPickup
{
	unsigned char	nItem;		// an INV_ id
	unsigned char	nName;		// an index into the IDS_ITEM_ ids, BloodModPickup_NameId
	char*			szModel;
	char*			szName;
};

// Row nRow's values, the mod's or retail's; and its NameID's string id.
const BloodModItemPickup& BloodModPickup_Row(int nRow);
int BloodModPickup_NameId(int nName);

const char* BloodModPickup_Name(int nRow);		// "TheEyePU"
const char* BloodModPickup_Key(int nField);
std::string BloodModPickup_Value(int nRow, int nField, bool bRetail);

bool BloodModPickups_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModPickups_Clear();
std::string BloodModPickups_Summary();
std::string BloodModPickups_Reference();
void BloodModPickups_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODPICKUPS_H__
