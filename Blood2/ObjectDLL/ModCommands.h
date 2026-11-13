// ----------------------------------------------------------------------- //
//
// MODULE  : ModCommands.h
//
// PURPOSE : Blood II's command manager and Command object: NOLF1's command
//           language (libs/LT1Mod/lt1mod_command.h) over Blood II's own
//           trigger send and random draws
//
// ----------------------------------------------------------------------- //

#ifndef __MOD_COMMANDS_H__
#define __MOD_COMMANDS_H__

#include "cpp_engineobjects_de.h"
#include "cpp_server_de.h"
#include "B2BaseClass.h"

// Reads Mod\Weapons.txt into g_WeaponDefaults after the mod is mounted; prints what changed, or
// why nothing did. The client shell does the same into its own copy.
void ModWeapons_LoadFile();
// Reads Mod\Characters.txt into gCharacterValues and gCharacterSkins, likewise
void ModCharacters_LoadFile();
// Reads Mod\AI.txt, likewise; AI_Mgr applies it to each AI as it is created
void ModAI_LoadFile();
// Reads Mod\Ammo.txt into gAmmoValues (BloodModAmmo.h), likewise: the carrying caps, the pickups
// and FOCUS's regeneration, all the server's
void ModAmmo_LoadFile();
// Reads Mod\Rules.txt into gRules (BloodModRules.h), likewise: the loose constants the player,
// the items, the hits and the corpses read
void ModRules_LoadFile();
// Reads Mod\Explosions.txt (BloodModExplosions.h), likewise: the server needs only its names,
// which number a mod's new explosions as the client numbers them. Before the projectile and
// weapon classes, which name explosions.
void ModExplosions_LoadFile();
// Reads Mod\Projectiles.txt (BloodModProjectiles.h), likewise: CProjectile applies it to each
// projectile as it is created
void ModProjectiles_LoadFile();
void ModPickups_LoadFile();		// Mod\Pickups.txt
// Reads Mod\WeaponClasses.txt (BloodModWeaponClasses.h), likewise: CWeapon::Init and the weapon
// pickups apply it
void ModWeaponClasses_LoadFile();
// Reads Mod\Commands.txt through the engine, after the mod is mounted; prints what it read, or
// why not
void ModCommands_LoadFile();
// Runs a command string now (the Command object; a mod's start commands)
void ModCommands_Process(const char* pCmd);
// Once a server frame, single player included
void ModCommands_Update(DFLOAT fFrameTime);
// A world is starting: drops what the last one left pending and, unless this is a restored
// save, runs the start commands for it
void ModCommands_WorldStarted(const char* pWorld, DBOOL bRestoring);
// The pending commands, into and out of the player's save, when a mod has commands; a retail
// session writes nothing. bApply is false for a keep-alive load (a level change), which comes
// after the new world's start commands.
void ModCommands_Save(HMESSAGEWRITE hWrite);
void ModCommands_Load(HMESSAGEREAD hRead, DBOOL bApply);

// A level object that runs a command string: Command on any trigger message, StartCommand once,
// just after the world starts. A new class, so no retail level uses it.
class Command : public B2BaseClass
{
	public :

		Command();
		~Command();

	protected :

		DDWORD EngineMessageFn(DDWORD messageID, void *pData, DFLOAT fData);
		DDWORD ObjectMessageFn(HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead);

	private :

		HSTRING	m_hstrCommand;
		HSTRING	m_hstrStartCommand;
		DBOOL	m_bStarted;
};


// A level object that calls a script function when triggered: its Function
// property names a global of the mod's Mod\Script.lua.
class ScriptTrigger : public B2BaseClass
{
	public :
		ScriptTrigger();
		~ScriptTrigger();
	protected :
		DDWORD EngineMessageFn(DDWORD messageID, void *pData, DFLOAT fData);
		DDWORD ObjectMessageFn(HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead);
	private :
		HSTRING	m_hstrFunction;
};

#endif // __MOD_COMMANDS_H__
