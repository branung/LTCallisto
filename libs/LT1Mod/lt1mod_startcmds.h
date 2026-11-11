#ifndef __LT1MOD_STARTCMDS_H__
#define __LT1MOD_STARTCMDS_H__

// A mod's start commands, Mod\Commands.txt, in lt1mod_command.h's language:
//
//     [Commands]
//     Trace = TRUE                   print every message a command sends
//
//     [Global]                       every world, first
//     Start = "MSG intro PLAY"
//
//     [World_01_Ambush]              one world: World_ and its file name
//     Start = "DELAY 5.0 (MSG (door1 door2) TRIGGER)"
//
// A world's section is World_ and its file name without folder or extension, in any case. The
// prefix is there because ButeMgr refuses a tag that begins with a digit, and Shogo's world
// names do. Any other section is an error.
//
// Start commands run once when a world starts (a new game or a level change, not a restored
// save, whose pending commands come back with it). The game's server shell runs them; this
// only reads the file. Each module that links the library has its own copy.

#include <string>
#include <vector>

namespace LT1Mod
{

class FileSource;

// Loads Mod\Commands.txt through src, with the tables' rules: absent is nothing to run; any
// error loads nothing and says where; no active mod clears.
bool StartCommands_Load(FileSource &src, std::string &sError);
void StartCommands_Clear();
bool StartCommands_Loaded();
// [Global]'s Start, then the world's own. pWorld may carry a folder and an extension, or be
// NULL for [Global]'s alone.
std::vector<std::string> StartCommands_For(const char *pWorld);
// [Commands] Trace
bool StartCommands_Trace();
// One line per section, for the console
std::string StartCommands_Summary();

} // namespace LT1Mod

#endif // __LT1MOD_STARTCMDS_H__
