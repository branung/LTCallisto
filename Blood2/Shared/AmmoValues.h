// AmmoValues.h: the ammo types' table (AmmoValues.cpp).
//
// Retail's constants by ammo type, out of the code that held them so that Mod\Ammo.txt
// (BloodModAmmo.h) can change them: ObjectDLL/InventoryMgr.cpp's fMaxAmmoCount and
// g_fFocusRegenAmt, Shared/SharedDefs.h's AMMOCOUNT_ defines, which the ammo pickups'
// constructors (ObjectDLL/AmmoPickups.cpp) give, the ammo types ObjectDLL/Weapon.cpp ejected a
// casing for, BULLET and SHELL, and the casing and muzzle smoke the client draws, from
// ClientShellDLL/ShellCasingFX.cpp and WeaponFX.cpp. Both modules read it, each its own fields.

#ifndef __AMMOVALUES_H__
#define __AMMOVALUES_H__

#include "SharedDefs.h"

struct AmmoVec { DFLOAT x, y, z; };		// BloodModTable's K_VECTOR

struct AmmoValues
{
	DFLOAT	fMax[6];	// the most a player carries, by strength 1-6, or magic for FOCUS
	DFLOAT	fPickup;	// what its ammo pickup gives; 0 where it has none
	DFLOAT	fRegen[6];	// FOCUS only: what it regains a second, by magic 1-6
	DBOOL	bEjectShell;	// a weapon firing it ejects a shell casing

	// The client's: the casing a shot ejects, and the smoke at the muzzle
	char*		szCasingModel;		// "" for none: no casing is drawn
	char*		szCasingSkin;
	AmmoVec		vCasingScale;
	DDWORD		nCasings;			// casings a shot ejects
	AmmoVec		vCasingVelMin;		// up, right, forward, each a random
	AmmoVec		vCasingVelMax;		// value between the two
	DFLOAT		fSmokeTime;			// the muzzle smoke's seconds
	DFLOAT		fSmokeAlpha;
	DFLOAT		fSmokeScaleMin;
	DFLOAT		fSmokeScaleMax;
	DBOOL		bSmokeSideways;		// drifts out to the side, as a shotgun's
};

// By AMMO_ id, AMMO_NONE's all zeros.
extern AmmoValues gAmmoValues[AMMO_MAXAMMOTYPES + 1];

// The ammo types that have a pickup (AmmoPickups.cpp), and the one that regenerates: where a
// mod's Pickup or Regen means anything.
inline bool AmmoHasPickup(int nType) { return nType >= AMMO_BULLET && nType <= AMMO_BATTERY; }
inline bool AmmoRegenerates(int nType) { return nType == AMMO_FOCUS; }

#endif // __AMMOVALUES_H__
