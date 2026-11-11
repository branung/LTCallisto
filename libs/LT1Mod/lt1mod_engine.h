#ifndef __LT1MOD_ENGINE_H__
#define __LT1MOD_ENGINE_H__

// The engine's mounted resource trees as a FileSource. OpenFile is on ILTCSBase, but
// GetFileList and FreeFileList are separate members of ILTServer and ILTClient, so the caller
// passes them in:
//
//     LT1Mod::EngineSource src(g_pLTServer, g_pLTServer->GetFileList, g_pLTServer->FreeFileList);
//
// Both calls see the trees in the engine's priority order (the last -rez wins, per file), so a
// mod reads the same copy of each file the game will.

#include "lt1mod.h"

class ILTCSBase;
struct FileEntry;

namespace LT1Mod
{

class EngineSource : public FileSource
{
public:
    typedef ::FileEntry* (*GetFileListFn)(const char* pDirName);
    typedef void (*FreeFileListFn)(::FileEntry* pHead);

    EngineSource(ILTCSBase* pBase, GetFileListFn getList, FreeFileListFn freeList)
        : m_pBase(pBase), m_GetFileList(getList), m_FreeFileList(freeList) {}

    virtual bool ReadFile(const char* path, std::string& out);
    virtual void ListDir(const char* dir, std::vector<DirEntry>& out);

private:
    ILTCSBase*      m_pBase;
    GetFileListFn   m_GetFileList;
    FreeFileListFn  m_FreeFileList;
};

// Load() through an EngineSource, then SetActive(). Each game module calls this once its engine
// interface is set.
const State& LoadActive(ILTCSBase* pBase, EngineSource::GetFileListFn getList,
                        EngineSource::FreeFileListFn freeList, const char* game);

} // namespace LT1Mod

#endif // __LT1MOD_ENGINE_H__
