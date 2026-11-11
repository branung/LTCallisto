#ifndef __LT1MOD_H__
#define __LT1MOD_H__

// Data driven mods for the LithTech 1.0 games (Shogo, Blood II). 
// A mod is a rez or a directory whose root holds Mod\Mod.txt, an attribute file:
//
//     [Mod]
//     Id      = "LOF"                      // required; letters, digits, _ -
//     Game    = "Shogo"                    // required; must name this game
//     Name    = "Legacy of the Fallen"     // optional; defaults to Id
//     Version = "0.1"                      // optional
//
// No retail archive has a top-level Mod directory, so retail content is never taken for a mod.
// With no Mod\Mod.txt mounted nothing changes: Load() reports "not present", Active() stays
// empty and SavePath() returns its argument.
//
// The interface has no engine or ButeMgr types, so the 1998 headers a game compiles against
// never meet ButeMgr's. lt1mod_engine.h is the one engine-facing piece.

#include <stdint.h>
#include <string>
#include <vector>

namespace LT1Mod
{

extern const char* const kModDir;           // "Mod"
extern const char* const kManifestPath;     // "Mod\\Mod.txt"

// One directory listing entry. path is the full '\\'-separated path, as GetFileList reports it.
struct DirEntry
{
    std::string path;
    bool        isDir;
};

// Where a mod's files are read from. The engine source reads the mounted trees, so it sees the
// highest-priority copy of each file, as the game does.
class FileSource
{
public:
    virtual ~FileSource() {}

    // Whole-file read; false if the file is missing or unreadable
    virtual bool ReadFile(const char* path, std::string& out) = 0;

    // One directory level, files and directories; an absent directory gives an empty list
    virtual void ListDir(const char* dir, std::vector<DirEntry>& out) = 0;
};

struct Manifest
{
    std::string id;
    std::string name;
    std::string game;
    std::string version;
    // [Options] ScriptTrace = TRUE prints every script event and API call
    bool        scriptTrace;

    Manifest() : scriptTrace(false) {}
};

struct State
{
    bool        present;    // Mod\Mod.txt exists in the mounted trees
    bool        valid;      // and parsed and passed validation
    std::string error;      // why not valid; empty when valid or absent
    Manifest    manifest;

    // FNV-1a 64 over every file under Mod\, which two machines compare before playing together
    uint64_t    hash;
    uint32_t    fileCount;

    State() : present(false), valid(false), hash(0), fileCount(0) {}
};

// Reads Mod\Mod.txt and hashes Mod\. `game` ("Shogo", "Blood2") is compared case-insensitively
// with the manifest's Game. Never fails: the outcome is in `out`, and an absent manifest is the
// normal unmodded case.
void Load(FileSource& src, const char* game, State& out);

// The module's active mod. The object module and the client shell each load and set their own.
const State& Active();
void         SetActive(const State& state);
bool         IsActive();                    // present && valid

// One line for a console: "no mod", "mod LOF 0.1 (Legacy of the Fallen), 12 files, hash
// 0123456789abcdef", or "mod error: <why>"
std::string Describe(const State& state);

// What a server and a client compare at connect: "none", "LOF:<hash>", or "invalid:<hash>" for a
// manifest that failed validation, so two machines with the same broken mod still agree. At most
// 64 characters.
std::string Signature(const State& state);

// A save path for the active mod. With none, `path` comes back unchanged. With one, a leading
// "Save\\" becomes "Save\\<Id>\\", so a mod never loads another's or retail's saves. A path
// already under Save\<Id>\ is unchanged, so mapping on the client and again on the server
// lands in the same place.
std::string SavePath(const char* path);

// The directory SavePath puts a mod's saves in ("Save\\LOF"), or empty with no active mod.
// A game creates it before its first save.
std::string SaveDir();

} // namespace LT1Mod

#endif // __LT1MOD_H__
