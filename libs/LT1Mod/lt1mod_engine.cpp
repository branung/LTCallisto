// LT1Mod::EngineSource

#include "iltcsbase.h"
#include "iltstream.h"

#include "lt1mod_engine.h"

namespace LT1Mod
{

namespace
{

// Native separators, as CHelpers::FormatFilename makes them. The client formats every name itself,
// but the server's OpenFile2 doesn't, so on Linux the server missed "Mod\Mod.txt" in a directory
// mount and single player disconnected itself as a mod mismatch.
std::string NativePath(const char* path)
{
    std::string s(path);
    for (size_t i = 0; i < s.size(); ++i)
    {
#if defined(__LINUX)
        if (s[i] == '\\')
            s[i] = '/';
#else
        if (s[i] == '/')
            s[i] = '\\';
#endif
    }
    return s;
}

} // namespace

// The whole file in one read; the size bound only keeps a corrupt length from asking for gigabytes
bool EngineSource::ReadFile(const char* path, std::string& out)
{
    const uint32 kMaxFileSize = 64u << 20;

    out.clear();
    if (!m_pBase || !path)
        return false;

    ILTStream* pStream = 0;
    if (m_pBase->OpenFile(NativePath(path).c_str(), &pStream) != LT_OK || !pStream)
        return false;

    uint32 len = 0;
    bool bOk = pStream->GetLen(&len) == LT_OK && len <= kMaxFileSize;
    if (bOk && len)
    {
        out.resize(len);
        bOk = pStream->Read(&out[0], len) == LT_OK && pStream->ErrorStatus() == LT_OK;
    }
    pStream->Release();
    if (!bOk)
        out.clear();
    return bOk;
}

void EngineSource::ListDir(const char* dir, std::vector<DirEntry>& out)
{
    out.clear();
    if (!m_GetFileList || !dir)
        return;
    ::FileEntry* pHead = m_GetFileList(NativePath(dir).c_str());
    for (::FileEntry* p = pHead; p; p = p->m_pNext)
    {
        DirEntry e;
        e.path = p->m_pFullFilename ? p->m_pFullFilename : "";
        e.isDir = p->m_Type == TYPE_DIRECTORY;
        if (!e.path.empty())
            out.push_back(e);
    }
    if (pHead && m_FreeFileList)
        m_FreeFileList(pHead);
}

const State& LoadActive(ILTCSBase* pBase, EngineSource::GetFileListFn getList,
                        EngineSource::FreeFileListFn freeList, const char* game)
{
    EngineSource src(pBase, getList, freeList);
    State s;
    Load(src, game, s);
    SetActive(s);
    return Active();
}

} // namespace LT1Mod
