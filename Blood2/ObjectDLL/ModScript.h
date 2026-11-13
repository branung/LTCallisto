// ----------------------------------------------------------------------- //
//
// MODULE  : ModScript.h
//
// PURPOSE : A mod's Mod\Script.lua in Blood 2's server
//
// The game side of the scripting layer, as Shogo's: the host the script
// reaches (the console, the command manager, the game's time, an AI's health
// and position), and the calls that raise its events. Object events come from
// the mod's own objects (AIs made as an AI.txt archetype) and from the AIs of a
// class the mod names with AI.txt's Script = TRUE; never from a retail enemy the
// mod did not name.
//
// ----------------------------------------------------------------------- //

#ifndef __MOD_SCRIPT_H__
#define __MOD_SCRIPT_H__

#include "cpp_server_de.h"

class CBaseCharacter;

// Loads Mod\Script.lua and prints what it handles, or why it was refused.
void ModScript_Load();
// A world has started; nothing on a restored save, as for the start commands.
void ModScript_WorldStarted(const char* pWorld, DBOOL bRestoring);
// The object events, for an AI the script hears (AI_Mgr::ModAI_Scripted): pArchetype is its
// archetype, "" for a retail class the mod opted in, and NULL no event. bPlaced: the level made
// it. hFrom, hKiller: the attacker, passed on if the script knows it; nType: the DAMAGE_TYPE_,
// named for it; fDamage: the killing blow's own amount.
void ModScript_Created(CBaseCharacter* pChar, const char* pArchetype, DBOOL bPlaced = DFALSE);
void ModScript_Damaged(CBaseCharacter* pChar, const char* pArchetype, DFLOAT fAmount, HOBJECT hFrom = DNULL, int nType = -1);
void ModScript_Died(CBaseCharacter* pChar, const char* pArchetype, HOBJECT hKiller = DNULL, int nType = -1,
					DFLOAT fDamage = -1.0f);
// Version 2 (lt1mod_script.h): a trigger message, with its sender's name.
void ModScript_Triggered(CBaseCharacter* pChar, const char* pArchetype, const char* pMsg, HOBJECT hSender);
// Before an object's memory goes, for any object a script may hold.
void ModScript_Removed(void* pObj);
// The players: pName is the player's name.
void ModScript_PlayerSpawned(CBaseCharacter* pPlayer, const char* pName);
void ModScript_PlayerDied(CBaseCharacter* pPlayer, const char* pName);
void ModScript_PlayerJoined(CBaseCharacter* pPlayer, const char* pName);
void ModScript_PlayerLeft(CBaseCharacter* pPlayer, const char* pName);
// A message from that player's client script (MSG_MOD_SCRIPT): OnClientMessage,
// within the size cap and the player's rate.
void ModScript_ClientMessage(CBaseCharacter* pPlayer, const char* pPlayerName, const char* pName, const char* pPayload);
// A restored world's mod object, known to the script again (no event).
void ModScript_Adopted(CBaseCharacter* pChar, const char* pArchetype);
// The script's field of the player's save, beside the commands';
// written whenever the mod has a script. bApply false (a level change's
// keep-alive save) reads it and keeps nothing: mod.save crosses a level change
// in memory.
void ModScript_Save(HMESSAGEWRITE hWrite);
void ModScript_Load(HMESSAGEREAD hRead, DBOOL bApply);
// The world is ending (the next one's PreStartWorld), and once a server frame.
void ModScript_WorldEnded();
void ModScript_Update();

#endif // __MOD_SCRIPT_H__
