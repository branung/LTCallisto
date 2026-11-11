// The mod manifest, its hash, and the module's active mod

#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lt1mod.h"
#include "lt1mod_attrib.h"

namespace LT1Mod
{

const char* const kModDir       = "Mod";
const char* const kManifestPath = "Mod\\Mod.txt";

namespace
{

const char* const kManifestTag = "Mod";

// An Id becomes a directory name and part of the connect signature
const size_t kMaxIdLen = 31;

// Bounds on a recursive walk over user-supplied data, far above any real mod
const int      kMaxDepth = 16;
const uint32_t kMaxFiles = 16384;

State g_Active;

bool EqualNoCase(const char* a, const char* b)
{
    for (;; ++a, ++b)
    {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return false;
        if (!ca) return true;
    }
}

bool ValidId(const std::string& id)
{
    if (id.empty() || id.size() > kMaxIdLen)
        return false;
    for (size_t i = 0; i < id.size(); ++i)
    {
        unsigned char c = (unsigned char)id[i];
        if (!isalnum(c) && c != '_' && c != '-')
            return false;
    }
    return true;
}

// Lower case and '\\'-separated, so a rez and a directory that spell a name differently hash alike
std::string Normalize(const std::string& path)
{
    std::string s(path);
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '/')
            s[i] = '\\';
        else
            s[i] = (char)tolower((unsigned char)s[i]);
    }
    return s;
}

void CollectFiles(FileSource& src, const std::string& dir, int depth,
                  std::vector<std::string>& files)
{
    if (depth > kMaxDepth || files.size() >= kMaxFiles)
        return;
    std::vector<DirEntry> entries;
    src.ListDir(dir.c_str(), entries);
    for (size_t i = 0; i < entries.size() && files.size() < kMaxFiles; ++i)
    {
        if (entries[i].isDir)
            CollectFiles(src, entries[i].path, depth + 1, files);
        else
            files.push_back(entries[i].path);
    }
}

bool LessNormalized(const std::pair<std::string, std::string>& a,
                    const std::pair<std::string, std::string>& b)
{
    return a.first < b.first;
}

// FNV-1a 64: a fingerprint two machines compare, not a security measure
struct Fnv64
{
    uint64_t h;
    Fnv64() : h(14695981039346656037ULL) {}
    void Add(const void* p, size_t n)
    {
        const unsigned char* b = (const unsigned char*)p;
        for (size_t i = 0; i < n; ++i)
        {
            h ^= b[i];
            h *= 1099511628211ULL;
        }
    }
    void AddU64(uint64_t v)
    {
        unsigned char b[8];
        for (int i = 0; i < 8; ++i)
            b[i] = (unsigned char)(v >> (8 * i));
        Add(b, 8);
    }
};

// Every file under Mod\ in normalized-path order: the path, a zero byte, the length as 8
// little-endian bytes, the contents. A file that lists but can't be read hashes with length ~0.
void HashModTree(FileSource& src, State& out)
{
    std::vector<std::string> files;
    CollectFiles(src, kModDir, 0, files);

    std::vector<std::pair<std::string, std::string> > sorted;
    for (size_t i = 0; i < files.size(); ++i)
        sorted.push_back(std::make_pair(Normalize(files[i]), files[i]));
    std::stable_sort(sorted.begin(), sorted.end(), LessNormalized);

    Fnv64 fnv;
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        std::string data;
        bool bRead = src.ReadFile(sorted[i].second.c_str(), data);
        fnv.Add(sorted[i].first.data(), sorted[i].first.size());
        fnv.Add("", 1);
        fnv.AddU64(bRead ? (uint64_t)data.size() : ~(uint64_t)0);
        fnv.Add(data.data(), data.size());
    }
    out.hash = fnv.h;
    out.fileCount = (uint32_t)sorted.size();
}

std::string Hex64(uint64_t v)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%08x%08x", (unsigned)(v >> 32), (unsigned)(v & 0xffffffffu));
    return buf;
}

// A key present with the wrong type is an error even when optional: `Id = LOF` without quotes
// is a typo to report, not a missing Id
bool ReadString(const Attributes& a, const char* key, bool bRequired,
                std::string& out, std::string& err)
{
    if (!a.Has(kManifestTag, key))
    {
        if (bRequired)
            err = std::string(kManifestPath) + ": [Mod] has no " + key;
        return !bRequired;
    }
    std::string before = a.Error();
    out = a.GetString(kManifestTag, key, "");
    if (a.Error() != before)
    {
        err = std::string(kManifestPath) + ": " + a.Error();
        return false;
    }
    return true;
}

} // namespace

void Load(FileSource& src, const char* game, State& out)
{
    out = State();

    std::string text;
    if (!src.ReadFile(kManifestPath, text))
        return;
    out.present = true;

    // Hashed whatever the manifest says, so two machines with the same broken mod still match
    HashModTree(src, out);

    Attributes a;
    if (!a.Parse(text.data(), text.size(), kManifestPath))
    {
        out.error = a.Error();
        return;
    }
    if (!a.Has(kManifestTag))
    {
        out.error = std::string(kManifestPath) + ": no [Mod] section";
        return;
    }

    Manifest m;
    std::string err;
    if (!ReadString(a, "Id", true, m.id, err) ||
        !ReadString(a, "Game", true, m.game, err) ||
        !ReadString(a, "Name", false, m.name, err) ||
        !ReadString(a, "Version", false, m.version, err))
    {
        out.error = err;
        return;
    }
    if (!ValidId(m.id))
    {
        out.error = std::string(kManifestPath) + ": Id \"" + m.id +
                    "\" must be 1-31 letters, digits, '_' or '-'";
        return;
    }
    if (!game || !EqualNoCase(m.game.c_str(), game))
    {
        out.error = std::string(kManifestPath) + ": this mod is for " + m.game +
                    ", not " + (game ? game : "this game");
        return;
    }
    if (m.name.empty())
        m.name = m.id;
    m.scriptTrace = a.GetBool("Options", "ScriptTrace", false);

    out.manifest = m;
    out.valid = true;
}

const State& Active()
{
    return g_Active;
}

void SetActive(const State& state)
{
    g_Active = state;
}

bool IsActive()
{
    return g_Active.present && g_Active.valid;
}

std::string Describe(const State& s)
{
    if (!s.present)
        return "no mod";
    if (!s.valid)
        return "mod error: " + s.error;
    char files[32];
    snprintf(files, sizeof(files), "%u file%s", (unsigned)s.fileCount, s.fileCount == 1 ? "" : "s");
    std::string d = "mod " + s.manifest.id;
    if (!s.manifest.version.empty())
        d += " " + s.manifest.version;
    if (s.manifest.name != s.manifest.id)
        d += " (" + s.manifest.name + ")";
    return d + ", " + files + ", hash " + Hex64(s.hash);
}

std::string Signature(const State& s)
{
    if (!s.present)
        return "none";
    if (!s.valid)
        return "invalid:" + Hex64(s.hash);
    return s.manifest.id + ":" + Hex64(s.hash);
}

std::string SaveDir()
{
    if (!IsActive())
        return std::string();
    return "Save\\" + g_Active.manifest.id;
}

std::string SavePath(const char* path)
{
    if (!path)
        return std::string();
    if (!IsActive())
        return path;
    // "Save\\" or "Save/", any case
    if (strlen(path) >= 5 && tolower((unsigned char)path[0]) == 's' &&
        tolower((unsigned char)path[1]) == 'a' && tolower((unsigned char)path[2]) == 'v' &&
        tolower((unsigned char)path[3]) == 'e' && (path[4] == '\\' || path[4] == '/'))
    {
        // A path already mapped stays as it is, so mapping twice is mapping once
        const std::string& id = g_Active.manifest.id;
        const char* rest = path + 5;
        if (strlen(rest) > id.size() && (rest[id.size()] == '\\' || rest[id.size()] == '/'))
        {
            bool bSame = true;
            for (size_t i = 0; i < id.size() && bSame; ++i)
                bSame = tolower((unsigned char)rest[i]) == tolower((unsigned char)id[i]);
            if (bSame)
                return SaveDir() + "\\" + (rest + id.size() + 1);
        }
        return SaveDir() + "\\" + rest;
    }
    return path;
}

} // namespace LT1Mod
