// A mod's archetypes as class names, so a level can place one by name.
//
// After a module's native classes this adds one class per archetype: a copy of its host's
// ClassDef under the archetype's name, with the host as parent and no functions or properties
// of its own, so the engine gives it the parent's construct and destruct functions and every
// property. An object of it runs its host's code; the host reads its class name at
// MID_PRECREATE and, finding an archetype of that name, takes it as the Archetype the level did
// not give.
//
// The game's ObjectDLLSetup calls this after reading its mod. The engine's class list cannot
// change afterwards, so a mod's archetypes are read once, when the module is bound.

#ifndef __LT1MOD_CLASSDEFS_H__
#define __LT1MOD_CLASSDEFS_H__

#include <string>
#include <utility>
#include <vector>

struct ClassDef;

namespace LT1Mod
{

// An archetype and the native class it is for
typedef std::pair<std::string, std::string> ClassName;

// Writes at pOut, at most nRoom, a ClassDef* for each entry whose host is one of the nNative
// classes at pNative and whose own name no native class has (both without case), and returns
// how many. sAdded lists the classes made ("Name (Host)", comma separated); sSkipped the
// archetypes left reachable only through their host, with why. The ClassDefs live until the
// module is unloaded.
int AppendClasses(const std::vector<ClassName>& classes, ClassDef** pNative, int nNative,
                  ClassDef** pOut, int nRoom, std::string& sAdded, std::string& sSkipped);

} // namespace LT1Mod

#endif // __LT1MOD_CLASSDEFS_H__
