// BloodModPickups.cpp: see BloodModPickups.h

#include "server_de.h"
#include "SharedDefs.h"
#include "ClientRes.h"
#include "BloodModPickups.h"
#include "BloodModTable.h"

#include <stddef.h>
#include <stdio.h>
#include <string>

using namespace BloodMod;

namespace
{

const Entry kRows[] =
{
	{ BMPU_BINOCULARS, "BinocularsPU" }, { BMPU_THEEYE, "TheEyePU" }, { BMPU_FLASHLIGHT, "FlashlightPU" },
	{ BMPU_MEDKIT, "MedKitPU" }, { BMPU_NIGHTGOGGLES, "NightGogglesPU" }, { BMPU_PROXIMITIES, "ProximitiesPU" },
	{ BMPU_REMOTES, "RemotesPU" }, { BMPU_TIMEBOMB, "TimeBombPU" },
};
const int kNumRows = (int)(sizeof(kRows) / sizeof(kRows[0]));

const Named kItems[] =
{
	{ "MEDKIT", INV_MEDKIT }, { "FLASHLIGHT", INV_FLASHLIGHT }, { "NIGHTGOGGLES", INV_NIGHTGOGGLES },
	{ "THEEYE", INV_THEEYE }, { "BINOCULARS", INV_BINOCULARS }, { "PROXIMITY", INV_PROXIMITY },
	{ "REMOTE", INV_REMOTE }, { "TIMEBOMB", INV_TIMEBOMB }, { 0, 0 },
};
// The IDS_ITEM_ strings, by index: a DBYTE cannot hold the ids themselves.
const int kNameIds[] = { IDS_ITEM_FLASHLIGHT, IDS_ITEM_MEDKIT, IDS_ITEM_GOGGLES, IDS_ITEM_BINOCULARS, IDS_ITEM_EYE,
						 IDS_ITEM_PROX, IDS_ITEM_REMOTE, IDS_ITEM_TIME };
const Named kNames[] =
{
	{ "ITEM_FLASHLIGHT", 0 }, { "ITEM_MEDKIT", 1 }, { "ITEM_GOGGLES", 2 }, { "ITEM_BINOCULARS", 3 },
	{ "ITEM_EYE", 4 }, { "ITEM_PROX", 5 }, { "ITEM_REMOTE", 6 }, { "ITEM_TIME", 7 }, { 0, 0 },
};

// ItemPickups.cpp's constructors, transcribed.
BloodModItemPickup g_Pickups[BMPU_COUNT] =
{
	{ INV_BINOCULARS, 3, (char*)"Binoculars_pu", (char*)"Binoculars" },
	{ INV_THEEYE, 4, (char*)"TheEye_pu", (char*)"The Eye" },
	{ INV_FLASHLIGHT, 0, (char*)"Flashlight_pu", (char*)"Flashlight" },
	{ INV_MEDKIT, 1, (char*)"Medkit_pu", (char*)"Medkit" },
	{ INV_NIGHTGOGGLES, 2, (char*)"NightGoggles_pu", (char*)"Night Goggles" },
	{ INV_PROXIMITY, 5, (char*)"Proximities_pu", (char*)"Proximity Bomb" },
	{ INV_REMOTE, 6, (char*)"Remotes_pu", (char*)"Remote Bomb" },
	{ INV_TIMEBOMB, 7, (char*)"TimeBomb_pu", (char*)"Time Bomb" },
};

const Field kFields[] =
{
	{ "Item", 0, offsetof(BloodModItemPickup, nItem), K_NAMED, kItems },
	{ "Model", 0, offsetof(BloodModItemPickup, szModel), K_STRING, 0 },
	{ "Name", 0, offsetof(BloodModItemPickup, szName), K_STRING, 0 },
	{ "NameID", 0, offsetof(BloodModItemPickup, nName), K_NAMED, kNames },
};
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const Array kArrays[] = { { &g_Pickups[0], sizeof(BloodModItemPickup) } };

Table g_Table("Mod\\Pickups.txt", "a Blood 2 item pickup", "the item pickup classes' names (TheEyePU, MedKitPU, ...)",
			  "Blood2/ModReference/Pickups.txt", kArrays, 1, BMPU_COUNT, BMPU_BINOCULARS,
			  kRows, kNumRows, kFields, kNumFields);

// A charged item's class gives a charged item, a bomb's class a bomb: the
// classes pick themselves up differently (ItemPickupCharged, ItemPickupWeapon).
bool CheckRows(std::string& sError)
{
	for (int r = 0; r < BMPU_COUNT; ++r)
	{
		const bool bBomb = r >= BMPU_PROXIMITIES;
		const int n = g_Pickups[r].nItem;
		if (bBomb != (n == INV_PROXIMITY || n == INV_REMOTE || n == INV_TIMEBOMB))
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "Mod\\Pickups.txt: [%s]:Item: %s", kRows[r].pName,
					 bBomb ? "a bomb's pickup gives a bomb (PROXIMITY, REMOTE, TIMEBOMB)"
						   : "a charged item's pickup gives one (MEDKIT, FLASHLIGHT, NIGHTGOGGLES, THEEYE, BINOCULARS)");
			sError = buf;
			return false;
		}
	}
	return true;
}

} // namespace

const BloodModItemPickup& BloodModPickup_Row(int nRow)
{
	return g_Pickups[nRow >= 0 && nRow < BMPU_COUNT ? nRow : 0];
}
int BloodModPickup_NameId(int nName)
{
	return kNameIds[nName >= 0 && nName < (int)(sizeof(kNameIds) / sizeof(kNameIds[0])) ? nName : 0];
}
const char* BloodModPickup_Name(int nRow)						{ return g_Table.Name(nRow); }
const char* BloodModPickup_Key(int nField)						{ return g_Table.Key(nField); }
std::string BloodModPickup_Value(int nRow, int nField, bool bRetail)	{ return g_Table.Value(nRow, nField, bRetail); }
void BloodModPickups_Clear()									{ g_Table.Clear(); }
std::string BloodModPickups_Summary()							{ return g_Table.Summary(); }
std::string BloodModPickups_Reference()							{ return g_Table.Reference(); }

bool BloodModPickups_Load(LT1Mod::FileSource &src, std::string &sError)
{
	if (!g_Table.Load(src, sError))
		return false;
	if (!CheckRows(sError))
	{
		g_Table.Clear();
		return false;
	}
	return true;
}

void BloodModPickups_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModPickups_Load(src, sErr);
	if (!pPrint) return;
	PrintWrapped(bOk ? g_Table.Summary() : sErr,
				 std::string(pPrefix ? pPrefix : "") + (bOk ? "Mod\\Pickups.txt: " : ""), pPrint);
}
