// ----------------------------------------------------------------------- //
//
// MODULE  : ModClassDefs.h
//
// PURPOSE : A mod's enemy types and projectiles as class names
//
// ObjectDLLSetup reads the mod's AI.txt (BloodModAI.h) before it answers the
// engine and returns, after the native classes, one class per archetype
// (libs/LT1Mod/lt1mod_classdefs.h): the class at the root of its Base chain,
// under the archetype's name.  So a level, or a Spawner's string, can place
// the archetype itself.  AI_Mgr asks its class's name at MID_PRECREATE and,
// finding an archetype, applies the class it is built on and then the
// archetype.  An Archetype property the level gives still wins.
// The file is read once, when the engine binds the module, and an archetype
// named like a native class stays reachable through its class only.
//
// Mod\Projectiles.txt's new classes (BloodModProjectiles.h) are appended the same way after
// them: each its root retail class under its own name, which a weapon's ProjectileClass names.
//
// ----------------------------------------------------------------------- //

#ifndef __MOD_CLASSDEFS_H__
#define __MOD_CLASSDEFS_H__

struct ClassDef;
class ILTServer;

// ObjectDLLSetup's (ltengineobjects.cpp, LT_EXTRA_CLASSDEFS_FN in
// CMakeLists.txt): writes at pOut a ClassDef* per archetype that becomes a
// class, at most nRoom, and prints one line saying which.
int ModClassDefs_Append(ILTServer* pServer, ClassDef** pNative, int nNative, ClassDef** pOut, int nRoom);

#endif // __MOD_CLASSDEFS_H__
