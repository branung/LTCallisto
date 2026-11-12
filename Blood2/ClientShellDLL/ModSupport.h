// Blood II's client half of libs/LT1Mod. Its own file so it builds on Linux apart from
// BloodClientShell.cpp; the shell calls these and owns the reaction to a mismatch.

#ifndef __MODSUPPORT_H__
#define __MODSUPPORT_H__

#include "client_de.h"

// Loads the mod mounted with the game, if any, and makes it the client's active one; prints a
// line to the console when one is present. Then loads its Mod\Weapons.txt into g_WeaponDefaults.
// Called from OnEngineInitialized, after the resource trees are mounted.
void B2Mod_Init(CClientDE* pClientDE);

// Handles MSG_MOD_SIGNATURE from the server: answers with this client's signature and returns
// DTRUE if the two differ, after printing both to the console. The caller disconnects, outside
// the message dispatch.
DBOOL B2Mod_OnServerSignature(CClientDE* pClientDE, HMESSAGEREAD hMessage);

// Hands MSG_MOD_SCRIPT, the server script's message, to the client's script
void B2Mod_OnScriptMessage(HMESSAGEREAD hMessage);

// The text the shell puts in its message box on a mismatch
const char* B2Mod_MismatchText();

#endif // __MODSUPPORT_H__
