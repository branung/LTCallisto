// AmmoValues.h: the ammo types' table (AmmoValues.cpp).
//
// Retail's constants by ammo type, out of the code that held them so that Mod\Ammo.txt
// (BloodModAmmo.h) can change them: ObjectDLL/InventoryMgr.cpp's fMaxAmmoCount and
// g_fFocusRegenAmt, Shared/SharedDefs.h's AMMOCOUNT_ defines, which the ammo pickups'
// constructors (ObjectDLL/AmmoPickups.cpp) give, and the ammo types ObjectDLL/Weapon.cpp ejected
// a casing for, BULLET and SHELL. The server reads it; the client never did.

#ifndef __AMMOVALUES_H__
#define __AMMOVALUES_H__

#include "SharedDefs.h"

struct AmmoValues
{
	DFLOAT	fMax[6];	// the most a player carries, by strength 1-6, or magic for FOCUS
	DFLOAT	fPickup;	// what its ammo pickup gives; 0 where it has none
	DFLOAT	fRegen[6];	// FOCUS only: what it regains a second, by magic 1-6
	DBOOL	bEjectShell;	// a weapon firing it ejects a shell casing
};

// By AMMO_ id, AMMO_NONE's all zeros.
extern AmmoValues gAmmoValues[AMMO_MAXAMMOTYPES + 1];

// The ammo types that have a pickup (AmmoPickups.cpp), and the one that regenerates: where a
// mod's Pickup or Regen means anything.
inline bool AmmoHasPickup(int nType) { return nType >= AMMO_BULLET && nType <= AMMO_BATTERY; }
inline bool AmmoRegenerates(int nType) { return nType == AMMO_FOCUS; }

#endif // __AMMOVALUES_H__
