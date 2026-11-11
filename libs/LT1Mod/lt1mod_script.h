// A mod's script, Mod\Script.lua, on Lua 5.4 (libs/lua).
//
// Optional, and the last layer: the tables and archetypes are data, and a script is for logic
// they cannot express. The game calls named events, the script answers the ones it defines, the
// API it sees is versioned, and it runs sandboxed. In Mod\Script.lua:
//
//     boss = nil
//     function OnCreate(obj)
//         if obj:archetype() == "Stalker" then boss = obj end
//     end
//     function OnDeath(obj)
//         if obj == boss then mod.command("MSG door1 TRIGGER") end
//     end
//
// EVENTS, each a no-op unless the script defines it, with the API version that added it. Object
// events are raised only for the mod's own objects (a game's archetype hosts) and the players,
// never for a retail class, so a script cannot change what a retail object does.
//
//     OnWorldStart(world)        a world starts: new game or level change     1
//     OnCreate(obj)              one of the mod's objects is created          1
//     OnDamage(obj, amount)      ...loses `amount` hit points                 1
//     OnDeath(obj)               ...dies                                      1
//     OnCommand(name, args...)   a command string ran SCRIPT name args        1
//                                (lt1mod_command.h); the args are strings
//     OnWorldEnd(world)          the world is about to end                    2
//     OnRemove(obj)              a mod object is being removed; only its      2
//                                name, archetype and class are left to read
//     OnTrigger(obj, msg, from)  a trigger message reached a mod object;      2
//                                `from` is the sender's name, or ""
//     OnPlayerSpawn(player)  OnPlayerDeath(player)                            2
//     OnPlayerJoin(player)   OnPlayerLeave(player)                            2
//     OnUpdate(dt)               only after mod.set_update_rate(hz), at most  2
//                                10 a second
//     OnRestore(world)           a saved game is restored, instead of         3
//                                OnWorldStart
//     OnClientMessage(player, name, payload)                                  5
//
// API. mod.api is the version the game provides; a later one only adds.
//
//   1   mod.print(...)  mod.command(cmd)  mod.time()  mod.world
//       obj:valid()  obj:name()  obj:archetype()  obj:class()
//       obj:health()  obj:set_health(n)  obj:pos() -> x, y, z
//   2   mod.find(name)        a live mod object by its level name, or nil
//       mod.objects([arch])   the live mod objects (of an archetype), a list
//       mod.players()         the live players, a list
//       mod.after(s, fn)  mod.every(s, fn) -> timer; timer:cancel()
//       mod.random(lo, hi)    the game's own random integer, lo..hi
//       mod.random()          or a number in [0, 1)
//       mod.hud(player|nil, text)   a line on one player's screen, or all
//       mod.sound(path, x, y, z)    a one shot sound at a point
//       mod.set_update_rate(hz)     0 (off) to 10
//       obj:send(msg)  obj:teleport(x, y, z)  obj:remove()
//       obj:max_health()  obj:damage(n)  obj:is_player()
//   3   mod.save  mod.require_api(n)
//   4   mod.spawn(archetype, x, y, z [, props])   one of the mod's archetypes at a point; props
//                              is the engine's property string ("Name boss1;HitPoints 400").
//                              Raises OnCreate and returns the object. Only archetypes, so a
//                              script makes nothing retail could not place.
//   5   mod.client(player | nil, name [, payload])   a message to the client script (below)
//
// A ScriptTrigger level object calls the global function its Function property names, with the
// trigger message and the sender's name, when it is triggered.
//
// AN OBJECT IS A WEAK REFERENCE. The same object is the same Lua value in every event, so it
// can be kept, compared and used as a table key. Once the game removes it, valid() is false and
// every other method is an error ("the object is gone"); a script never reaches freed memory.
//
// SANDBOX. Lua's base, string, table, math, utf8 and coroutine libraries without dofile,
// loadfile, load and require: no io, os, package or debug, so a script reaches no file and
// nothing outside this API. math.random is the game's (mod.random). A call that runs past
// kScriptBudget instructions is stopped, and memory is capped at kScriptMemory. An error in a
// handler or timer is printed with its line, and that handler is off for the rest of the world.
//
// WHAT LASTS. A script's state is rebuilt for every world: at a world's end its chunk runs again
// in a fresh Lua state, before the next world's objects are created. mod.save is what lasts (a
// table of strings, numbers, booleans, tables and object references), carried from world to
// world and written into the player's save with the pending timers (a timer whose function is a
// global, by that name; any other is not saved, and says so). A restored game raises OnRestore
// with mod.save as saved, each object reference found again by its name and archetype, or nil.
//
// THE CLIENT. Each client shell can run a second script, Mod\ClientScript.lua, for what a
// player sees and presses; it never decides game state. Same library and sandbox, loaded with
// kScriptClient, with its own events and API. It has no objects and no mod.save, and its state
// lasts as long as the client shell, across worlds.
//
//     OnClientStart()            the shell has loaded the mod
//     OnWorldStart(world)        the client enters a world
//     OnDraw(w, h)               every frame, after the game's HUD; at most
//                                kScriptDrawBudget instructions a frame
//     OnCommand(name, args...)   the player ran `script name args` (a bound
//                                key, the console)
//
//     mod.print  mod.time  mod.world  mod.api  mod.require_api  mod.random
//     mod.after  mod.every  mod.set_update_rate (OnUpdate)
//     mod.hud.text(x, y, text [, r, g, b])     inside OnDraw only
//     mod.hud.rect(x, y, w, h, r, g, b)        inside OnDraw only
//     mod.sound(path)                          a sound on this client
//
// MESSAGES (version 5) are the only way between the two scripts: a name (1 to
// kScriptMessageName bytes) and a text payload (at most kScriptMessageMax bytes), carried on the
// game's own message id.
//
//     client   mod.server(name [, payload])  -> OnClientMessage on the server
//              OnServerMessage(name, payload)
//     server   mod.client(player | nil, name [, payload]) -> that player's
//              client, or every client; OnServerMessage there
//              OnClientMessage(player, name, payload)
//
// A client sends at most kScriptMessageRate a second, and the server takes at most that many
// from each player whatever the client did: one over the rate is dropped, counted and reported
// once a second, and mod.server answers false. The server's script receives an event, never a
// command, so a modified client can only ask. A message too large or holding a NUL is a script
// error where it is sent, and dropped where it arrives.
//
// DIAGNOSTICS:
//   - [Options] ScriptTrace = TRUE in Mod\Mod.txt prints every event as it is raised and every
//     API call as it is made, with their arguments. The mod's signature includes Mod.txt, so a
//     traced mod is a different mod to a server.
//   - An event or timer that runs longer than kScriptSlowMs by the wall clock is reported once
//     a world: "OnUpdate took 12.3 ms".
//   - Script_Lint, run by a mod checker: names each mod.<call> neither side has, or the other
//     side's, and each call or event above the version mod.require_api asks for, then the
//     version the script needs.
//
// Server scripts run where the game's objects do, in the server's module.

#ifndef __LT1MOD_SCRIPT_H__
#define __LT1MOD_SCRIPT_H__

#include <stdlib.h>
#include <string>

namespace LT1Mod
{

class FileSource;

// What a script reaches in the game. The game implements it; a test can too. An object is the
// game's own opaque handle, and every call is made only for an object the game has not yet
// reported removed.
class ScriptHost
{
public:
    virtual ~ScriptHost() {}
    virtual void   Print(const char* pLine) = 0;
    virtual void   Command(const char* pCmd) = 0;
    virtual double Time() = 0;
    virtual bool   GetHealth(void* hObj, float& fHealth) = 0;
    virtual bool   SetHealth(void* hObj, float fHealth) = 0;
    virtual bool   GetPos(void* hObj, float& x, float& y, float& z) = 0;
    // Version 2. The defaults answer "the game cannot", so a version 1 host still builds; the
    // call is then a script error.
    virtual bool   GetMaxHealth(void* hObj, float& f) { (void)hObj; (void)f; return false; }
    virtual bool   Damage(void* hObj, float fAmount) { (void)hObj; (void)fAmount; return false; }
    virtual bool   SendTrigger(void* hObj, const char* pMsg) { (void)hObj; (void)pMsg; return false; }
    virtual bool   Teleport(void* hObj, float x, float y, float z) { (void)hObj; (void)x; (void)y; (void)z; return false; }
    virtual bool   Remove(void* hObj) { (void)hObj; return false; }
    virtual int    RandomInt(int lo, int hi) { return lo + (hi > lo ? rand() % (hi - lo + 1) : 0); }
    // A line on one player's screen; NULL is every player
    virtual bool   Hud(void* hPlayer, const char* pText) { (void)hPlayer; (void)pText; return false; }
    // Version 5: a message to that player's client script; NULL is every client. False when it
    // cannot go (a player with no client).
    virtual bool   SendToClient(void* hPlayer, const char* pName, const char* pPayload)
    {
        (void)hPlayer; (void)pName; (void)pPayload;
        return false;
    }
    virtual bool   Sound(const char* pPath, float x, float y, float z) { (void)pPath; (void)x; (void)y; (void)z; return false; }
    // Version 4: creates an object of the mod's archetype at a point, with the engine's property
    // string; its handle, or NULL with sWhy when there is no such archetype or nothing was made
    virtual void*  Spawn(const char* pArchetype, float x, float y, float z, const char* pProps, std::string& sWhy)
    {
        (void)pArchetype; (void)x; (void)y; (void)z; (void)pProps;
        sWhy = "this game spawns nothing";
        return NULL;
    }
};

// The client's host (kScriptClient), drawing on the screen surface
class ScriptClientHost : public ScriptHost
{
public:
    virtual bool   GetHealth(void*, float&) { return false; }
    virtual bool   SetHealth(void*, float) { return false; }
    virtual bool   GetPos(void*, float&, float&, float&) { return false; }
    virtual void   Command(const char*) {}
    // Not "DrawText", a Win32 macro that windows.h would turn into DrawTextA
    virtual bool   DrawHudText(int x, int y, const char* pText, int r, int g, int b) = 0;
    virtual bool   DrawRect(int x, int y, int w, int h, int r, int g, int b) = 0;
    virtual bool   Sound2D(const char* pPath) { (void)pPath; return false; }
    // A message to the server's script
    virtual bool   SendToServer(const char* pName, const char* pPayload) { (void)pName; (void)pPayload; return false; }
};

enum ScriptSide { kScriptServer, kScriptClient };

// An object an event is about
struct ScriptObject
{
    void*       hObj;
    const char* pName;          // the level's Name, or a player's name; "" for none
    const char* pArchetype;     // its archetype; "" for a player
    const char* pClass;         // its class: the host's, or the archetype's own
};

const int    kScriptApi    = 5;
const size_t kScriptSaveMax = 64u << 10;    // mod.save, saved: bytes
const long   kScriptBudget = 10000000L;     // instructions per call
const size_t kScriptMemory = 16u << 20;     // bytes
const long   kScriptDrawBudget = 1000000L;  // OnDraw's instructions a frame
const int    kScriptMaxUpdateRate = 10;     // OnUpdate calls a second, at most
const size_t kScriptMessageMax  = 1024;     // a message's payload: bytes
const size_t kScriptMessageName = 64;       // a message's name: bytes
const int    kScriptMessageRate = 20;       // messages a second, from each client
const double kScriptSlowMs     = 5.0;       // an event longer than this is reported, once a world
// What a received message's two strings are read into: one byte past the limit plus the NUL, so
// a sender that wrote too much is seen to have and dropped, rather than cut to a size that passes
const unsigned kScriptMessageNameBuf    = kScriptMessageName + 2;
const unsigned kScriptMessagePayloadBuf = kScriptMessageMax + 2;

// Loads the side's script through src and runs its top level, with pHost as the game. Absent,
// or no active mod, is no script and no error; an error in the top level loads nothing and
// says where.
bool Script_Load(FileSource& src, ScriptHost* pHost, std::string& sError, ScriptSide side = kScriptServer);
// The client's: OnClientStart, and OnDraw with the screen's size. A frame's OnDraw past its
// budget is off for the rest of the world.
void Script_ClientStart();
void Script_Draw(int nWidth, int nHeight);
void Script_Clear();
bool Script_Loaded();
// The mod has a script, running or turned off by an error: a save writes its script field
// whenever this is true, so a save and its load agree
bool Script_Present();
// "handles OnWorldStart, OnDeath", and a warning per global function named On... that is no
// event ("OnDamaged is not an event")
std::string Script_Summary();
// Messages as the game received them. The server's, from a player's client, raises
// OnClientMessage unless the message is malformed or over that player's rate; the client's, from
// the server, raises OnServerMessage. Each returns whether the script got it; a dropped one is
// counted.
bool Script_ClientMessage(const ScriptObject& player, const char* pName, const char* pPayload);
bool Script_ServerMessage(const char* pName, const char* pPayload);
// Messages dropped since the script loaded, on either side
int  Script_MessagesDropped();

// The static pass over a script's text: one line per finding, "Mod\Script.lua:12: WARNING: ...",
// then one "Mod\Script.lua: needs mod.api N (...)" for the highest version it uses
std::string Script_Lint(const std::string& text, ScriptSide side);

// For a mod checker: loads the side's script with a host that has no world, so its top level
// runs and its prints are shown, prints its summary or error through pPrint, and clears it.
// False if it would not load; no script is true and prints nothing.
bool Script_Check(FileSource& src, ScriptSide side, void (*pPrint)(const char*));

// The world. WorldEnd also forgets every object and timer of that world and rebuilds the state.
// WorldStart with bRestoring raises OnRestore with what Script_RestoreBlob was given.
void Script_WorldStart(const char* pWorld, bool bRestoring = false);
void Script_WorldEnd();

// mod.save and the timers, for the player's save; empty with no script. RestoreBlob keeps a
// save's for the next WorldStart.
std::string Script_SaveBlob();
void Script_RestoreBlob(const std::string& blob);
// An object a restored world brings back: known to the script, no event
void Script_Adopt(const ScriptObject& o);
// Once a server frame: due timers, then OnUpdate at the script's rate
void Script_Update();

// The object events. The first event for an object registers it, so a host need not call
// Create for an object it made before the script loaded.
void Script_Create(const ScriptObject& o);
void Script_Damage(const ScriptObject& o, float fAmount);
void Script_Death(const ScriptObject& o);
void Script_Trigger(const ScriptObject& o, const char* pMsg, const char* pFrom);
// Before the object's memory goes: OnRemove, then the reference is dead
void Script_Removed(void* hObj);
// The players, as objects (pArchetype "")
void Script_PlayerSpawn(const ScriptObject& o);
void Script_PlayerDeath(const ScriptObject& o);
void Script_PlayerJoin(const ScriptObject& o);
void Script_PlayerLeave(const ScriptObject& o);
// SCRIPT <name> args: true when the script has an OnCommand to call
bool Script_Command(const char* pName, const char* const* pArgs, int nArgs);
// A ScriptTrigger: calls the global function pFunction with the string args; false when the
// script has no such function. An error is printed and that function is off until the next
// world, as a handler's.
bool Script_Function(const char* pFunction, const char* const* pArgs, int nArgs);

} // namespace LT1Mod

#endif // __LT1MOD_SCRIPT_H__
