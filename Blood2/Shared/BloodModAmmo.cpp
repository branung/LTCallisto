// BloodModAmmo.cpp: see BloodModAmmo.h

#include "server_de.h"
#include "SharedDefs.h"
#include "AmmoValues.h"
#include "BloodModAmmo.h"
#include "BloodModTable.h"

#include <stddef.h>
#include <stdio.h>
#include <string>

using namespace BloodMod;

namespace
{

#define A(x) { AMMO_##x, #x }
const Entry kAmmoNames[] =
{
	A(BULLET), A(SHELL), A(BMG), A(FLARE), A(DIEBUGDIE), A(HOWITZER), A(FUEL), A(BATTERY),
	A(FOCUS), A(PROXIMITYBOMB), A(REMOTEBOMB), A(TIMEBOMB),
};
#undef A
const int kNumNames = (int)(sizeof(kAmmoNames) / sizeof(kAmmoNames[0]));

#define M(key, i) { key, 0, offsetof(AmmoValues, fMax) + (i) * sizeof(DFLOAT), K_FLOAT, 0 }
#define R(key, i) { key, 0, offsetof(AmmoValues, fRegen) + (i) * sizeof(DFLOAT), K_FLOAT, 0 }
const Field kFields[] =
{
	M("Max1", 0), M("Max2", 1), M("Max3", 2), M("Max4", 3), M("Max5", 4), M("Max6", 5),
	{ "Pickup", 0, offsetof(AmmoValues, fPickup), K_FLOAT, 0 },
	R("Regen1", 0), R("Regen2", 1), R("Regen3", 2), R("Regen4", 3), R("Regen5", 4), R("Regen6", 5),
	{ "EjectShell", 0, offsetof(AmmoValues, bEjectShell), K_BOOL, 0 },
};
#undef M
#undef R
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

// Ids AMMO_BULLET .. AMMO_MAXAMMOTYPES, element id - 1 of gAmmoValues + 1.
const Array kArrays[] = { { &gAmmoValues[AMMO_BULLET], sizeof(AmmoValues) } };

Table g_Table("Mod\\Ammo.txt", "a Blood 2 ammo type", "the AMMO_ names without the prefix",
			  "Blood2/ModReference/Ammo.txt", kArrays, 1, AMMO_MAXAMMOTYPES, AMMO_BULLET,
			  kAmmoNames, kNumNames, kFields, kNumFields);

// A Pickup where there is no pickup, or a Regen where nothing regenerates, would be a value
// nothing reads, so it is refused
bool CheckKeys(std::string& sError)
{
	char buf[256];
	for (int nId = AMMO_BULLET; nId <= AMMO_MAXAMMOTYPES; ++nId)
	{
		const AmmoValues& v = gAmmoValues[nId];
		const char* pWhy = 0;
		if (!AmmoHasPickup(nId) && v.fPickup != 0.0f)
			pWhy = "Pickup: it has no ammo pickup (BULLET .. BATTERY have)";
		for (int i = 0; !pWhy && i < 6; ++i)
			if (!AmmoRegenerates(nId) && v.fRegen[i] != 0.0f)
				pWhy = "Regen: only FOCUS regenerates";
		if (pWhy)
		{
			snprintf(buf, sizeof(buf), "Mod\\Ammo.txt: [%s]:%s", g_Table.Name(nId), pWhy);
			sError = buf;
			return false;
		}
	}
	return true;
}

} // namespace

const char* BloodModAmmo_Name(int nId)							{ return g_Table.Name(nId); }
int BloodModAmmo_Find(const char* pName)						{ return g_Table.Find(pName); }
const char* BloodModAmmo_Key(int nField)						{ return g_Table.Key(nField); }
std::string BloodModAmmo_Value(int nId, int nField, bool bRetail)	{ return g_Table.Value(nId, nField, bRetail); }
void BloodModAmmo_Clear()										{ g_Table.Clear(); }
std::string BloodModAmmo_Summary()								{ return g_Table.Summary(); }
std::string BloodModAmmo_Reference()							{ return g_Table.Reference(); }

bool BloodModAmmo_Load(LT1Mod::FileSource &src, std::string &sError)
{
	if (!g_Table.Load(src, sError))
		return false;
	if (!CheckKeys(sError))
	{
		g_Table.Clear();
		return false;
	}
	return true;
}

void BloodModAmmo_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	std::string sErr;
	const bool bOk = BloodModAmmo_Load(src, sErr);
	if (!pPrint) return;
	PrintWrapped(bOk ? g_Table.Summary() : sErr,
				 std::string(pPrefix ? pPrefix : "") + (bOk ? "Mod\\Ammo.txt: " : ""), pPrint);
}
