// What testBlood2Tables' tables share (main.cpp defines them)

#ifndef __BLOOD2TABLES_HARNESS_H__
#define __BLOOD2TABLES_HARNESS_H__

#include "lt1mod.h"

#include <map>
#include <string>
#include <vector>

extern int g_nPass, g_nFail;
void Check(bool b, const std::string& what);

// A mod in memory, for LT1Mod's FileSource
class MemSource : public LT1Mod::FileSource
{
public:
	std::map<std::string, std::string> files;
	virtual bool ReadFile(const char* path, std::string& out);
	virtual void ListDir(const char* dir, std::vector<LT1Mod::DirEntry>& out);
};

// Mounts a mod holding pPath = text (none when pPath is NULL), makes it the active mod, and loads
// it with pLoad, a table's _Load
bool MountAndLoad(const char* pPath, const std::string& text, std::string& err,
				  bool (*pLoad)(LT1Mod::FileSource&, std::string&));

// A value that differs from v, of the same kind, as the table files write it; v is the
// reference's text for key
std::string Other(const std::string& key, const std::string& v);

std::string Join(const std::vector<std::string>& v);
std::string Slurp(const char* pPath);

// characters.cpp: the selftest, and the reference file's text
void CharactersSelfTest();
std::string CharactersDump();
// ai.cpp: likewise for Mod\AI.txt
void AISelfTest();
// Every class ObjectDLL registers with pParent as its parent (ai.cpp).
#include <set>
std::set<std::string> ScanClasses(const char* pDir, std::string& sWhy, const char* pParent);
void AmmoSelfTest();
void ProjectilesSelfTest();
void WeaponClassesSelfTest();
std::string WeaponClassesDump();
void RulesSelfTest();
void ExplosionsSelfTest();
std::string ExplosionsDump();
void LasersSelfTest();
std::string LasersDump();
void ObjectFXSelfTest();
std::string ObjectFXDump();
void PickupsSelfTest();
std::string PickupsDump();
std::string RulesDump();
std::string ProjectilesDump();
std::string AmmoDump();
std::string AIDump();
// strings.cpp: Mod\Strings.txt, whose retail half is the built string module at g_CResPath (main
// sets it); StringsRetail is false without one
extern std::string g_CResPath;
void StringsSelfTest();
bool StringsRetail(std::map<int, std::string>& out);
std::string StringsDump(const std::map<int, std::string>& retail);

#endif // __BLOOD2TABLES_HARNESS_H__
