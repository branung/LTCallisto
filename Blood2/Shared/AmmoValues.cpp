// AmmoValues.cpp: see AmmoValues.h. Retail's values, moved unchanged: the rows are
// InventoryMgr.cpp's fMaxAmmoCount, the pickups SharedDefs.h's AMMOCOUNT_, FOCUS's regen
// InventoryMgr.cpp's g_fFocusRegenAmt, and EjectShell Weapon.cpp's
// `m_nAmmoType == AMMO_BULLET || m_nAmmoType == AMMO_SHELL`.

#include "server_de.h"
#include "SharedDefs.h"
#include "AmmoValues.h"

#define NONE6	{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }

AmmoValues gAmmoValues[AMMO_MAXAMMOTYPES + 1] =
{
	// Strength or Magic attrib:
	//   1		2		3		4		5		6
	{ NONE6, 0.0f, NONE6, DFALSE },															// None
	{ { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f }, 100.0f, NONE6, DTRUE },			// Bullets
	{ { 50.0f,  75.0f,  100.0f, 125.0f, 150.0f, 175.0f }, 20.0f,  NONE6, DTRUE },			// Shells
	{ { 20.0f,  40.0f,  60.0f,  80.0f,  100.0f, 120.0f }, 20.0f,  NONE6, DFALSE },			// BMG Rounds
	{ { 20.0f,  40.0f,  60.0f,  80.0f,  100.0f, 120.0f }, 20.0f,  NONE6, DFALSE },			// Flares
	{ { 25.0f,  50.0f,  75.0f,  100.0f, 150.0f, 200.0f }, 20.0f,  NONE6, DFALSE },			// DieBugDie
	{ { 5.0f,   10.0f,  20.0f,  35.0f,  50.0f,  120.0f }, 5.0f,   NONE6, DFALSE },			// Rockets
	{ { 25.0f,  25.0f,  50.0f,  75.0f,  100.0f, 125.0f }, 10.0f,  NONE6, DFALSE },			// Fuel
	{ { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f }, 40.0f,  NONE6, DFALSE },			// Batteries
	{ { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f }, 0.0f,						// Focus
	  { 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f }, DFALSE },
	{ { 10.0f,  10.0f,  10.0f,  10.0f,  10.0f,  10.0f },  0.0f,   NONE6, DFALSE },			// ProximityBomb
	{ { 10.0f,  10.0f,  10.0f,  10.0f,  10.0f,  10.0f },  0.0f,   NONE6, DFALSE },			// RemoteBomb
	{ { 10.0f,  10.0f,  10.0f,  10.0f,  10.0f,  10.0f },  0.0f,   NONE6, DFALSE },			// TimeBomb
};

#undef NONE6
