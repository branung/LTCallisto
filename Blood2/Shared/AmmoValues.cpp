// AmmoValues.cpp: see AmmoValues.h. Retail's values, moved unchanged: the rows are
// InventoryMgr.cpp's fMaxAmmoCount, the pickups SharedDefs.h's AMMOCOUNT_, FOCUS's regen
// InventoryMgr.cpp's g_fFocusRegenAmt, and EjectShell Weapon.cpp's
// `m_nAmmoType == AMMO_BULLET || m_nAmmoType == AMMO_SHELL`, and the client's casings and muzzle
// smoke, below.

#include "server_de.h"
#include "SharedDefs.h"
#include "AmmoValues.h"

#define NONE6	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }

// The casings (ShellCasingFX.cpp): a model and skin for BULLET and SHELL only, scale 2.25 for
// SHELL and 1.5 for any other, every casing launched at up 60-90, right 50-70, forward 10-25,
// and WeaponFX's two casings for a SHELL shot. The muzzle smoke
// (WeaponFX::CreateMuzzleSmoke): SHELL's branch, and every other type's.
#define CASE(model, skin, scale, n)	(char*)model, (char*)skin, { scale, scale, scale }, n, \
									{ 60.0f, 50.0f, 10.0f }, { 90.0f, 70.0f, 25.0f }
#define NOCASE		CASE("", "", 1.5f, 1)
#define SMOKE		1.5f, 0.5f, 0.05f, 0.25f, DFALSE
#define SHELLSMOKE	1.0f, 0.25f, 0.04f, 0.25f, DTRUE

AmmoValues gAmmoValues[AMMO_MAXAMMOTYPES + 1] =
{
	// Strength or Magic attrib:
	//   1		2		3		4		5		6
	{ NONE6, 0.0f, NONE6, DFALSE, NOCASE, SMOKE },														// None
	{ { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f }, 100.0f, NONE6, DTRUE,
	  CASE("models/ammo/bshell.abc", "skins/ammo/bshell.dtx", 1.5f, 1), SMOKE },							// Bullets
	{ { 50.0f,  75.0f,  100.0f, 125.0f, 150.0f, 175.0f }, 20.0f,  NONE6, DTRUE,
	  CASE("models/ammo/sshell.abc", "skins/ammo/sshell.dtx", 2.25f, 2), SHELLSMOKE },					// Shells
	{ { 20.0f,  40.0f,  60.0f,  80.0f,  100.0f, 120.0f }, 20.0f,  NONE6, DFALSE, NOCASE, SMOKE },		// BMG Rounds
	{ { 20.0f,  40.0f,  60.0f,  80.0f,  100.0f, 120.0f }, 20.0f,  NONE6, DFALSE, NOCASE, SMOKE },		// Flares
	{ { 25.0f,  50.0f,  75.0f,  100.0f, 150.0f, 200.0f }, 20.0f,  NONE6, DFALSE, NOCASE, SMOKE },		// DieBugDie
	{ { 5.0f,   10.0f,  20.0f,  35.0f,  50.0f,  120.0f }, 5.0f,   NONE6, DFALSE, NOCASE, SMOKE },		// Rockets
	{ { 25.0f,  25.0f,  50.0f,  75.0f,  100.0f, 125.0f }, 10.0f,  NONE6, DFALSE, NOCASE, SMOKE },		// Fuel
	{ { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f }, 40.0f,  NONE6, DFALSE, NOCASE, SMOKE },		// Batteries
	{ { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f }, 0.0f,											// Focus
	  { 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f }, DFALSE, NOCASE, SMOKE },
	{ { 10.0f,  10.0f,  10.0f,  10.0f,  10.0f,  10.0f },  0.0f,   NONE6, DFALSE, NOCASE, SMOKE },		// ProximityBomb
	{ { 10.0f,  10.0f,  10.0f,  10.0f,  10.0f,  10.0f },  0.0f,   NONE6, DFALSE, NOCASE, SMOKE },		// RemoteBomb
	{ { 10.0f,  10.0f,  10.0f,  10.0f,  10.0f,  10.0f },  0.0f,   NONE6, DFALSE, NOCASE, SMOKE },		// TimeBomb
};

#undef CASE
#undef NOCASE
#undef SMOKE
#undef SHELLSMOKE

#undef NONE6
