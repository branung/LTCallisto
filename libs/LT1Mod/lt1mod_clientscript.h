#ifndef __LT1MOD_CLIENTSCRIPT_H__
#define __LT1MOD_CLIENTSCRIPT_H__

// A mod's Mod\ClientScript.lua in a client shell (lt1mod_script.h, THE CLIENT).
//
// The client's script host on ILTClient, shared by both games' shells (a DE1 shell's CClientDE
// derives from ILTClient). It gives a script the console, the client's clock, text and rectangles
// on the screen surface inside OnDraw, and a local sound. It decides no game state.
//
// The shell calls:
//   - ClientScript_Load once, after the mod's tables are loaded. It registers the
//     `script <name> [args]` console command, which reaches OnCommand, and raises OnClientStart.
//   - ClientScript_PreLoadWorld with the world's name when the engine says which world it is
//     loading, and ClientScript_WorldStarted when the client has entered it (OnWorldStart).
//   - ClientScript_Update once a frame, for timers and OnUpdate.
//   - ClientScript_Draw inside the HUD's 2D block, after the game's HUD, so what a script draws
//     is over it and scales as the HUD does.
// With no Mod\ClientScript.lua each is a no-op.
//
// The shell owns its game's message id, so it passes ClientScript_Load the send function
// mod.server calls, and hands what the server sent to ClientScript_ServerMessage.

class ILTClient;

namespace LT1Mod
{

// pFontFace is the typeface mod.hud.text draws in: the game's HUD font.
// pSend sends a script message to the server, or answers false.
typedef bool (*ClientScriptSendFn)(const char* pName, const char* pPayload);
void ClientScript_Load(ILTClient* pClient, const char* pFontFace, ClientScriptSendFn pSend);
void ClientScript_ServerMessage(const char* pName, const char* pPayload);
void ClientScript_PreLoadWorld(const char* pWorld);
void ClientScript_WorldStarted();
void ClientScript_Update();
void ClientScript_Draw();

} // namespace LT1Mod

#endif // __LT1MOD_CLIENTSCRIPT_H__
