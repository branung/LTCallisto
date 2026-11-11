// testButeMerge: CButeMgr::Merge, a mod's bute overlay
//
//   testButeMerge --selftest
//   testButeMerge --retail <dir>    every *.txt in <dir> merged onto itself with every value unchanged

#include "stdafx.h"
#include "butemgr.h"
#include "l_allocator.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// stdlith leaves its allocator hooks to each module
void* DefStdlithAlloc(uint32 size)
{
    return size ? malloc((size_t)size) : NULL;
}

void DefStdlithFree(void* ptr)
{
    if (ptr)
        free(ptr);
}

namespace
{

int g_nPass = 0, g_nFail = 0;

void Check(bool cond, const std::string& what)
{
    printf("  %s  %s\n", cond ? "ok  " : "FAIL", what.c_str());
    (cond ? g_nPass : g_nFail)++;
}

bool ParseText(CButeMgr& b, const std::string& text)
{
    std::vector<char> buf(text.begin(), text.end());
    return b.Parse(buf.data(), (unsigned long)buf.size());
}

const char* kRetail =
    "[Weapon0]\n"
    "Name = \"Pistol\"\n"
    "Damage = 10\n"
    "Speed = 2.5\n"
    "[Weapon1]\n"
    "Name = \"Rifle\"\n"
    "Damage = 30\n"
    "Speed = 1.5\n";

int Selftest()
{
    printf("merge\n");
    {
        CButeMgr b, o;
        Check(ParseText(b, kRetail), "a retail file parses");
        Check(ParseText(o,
            "[Weapon0]\n"
            "Damage = 12.6\n"            // a double over an int
            "Extra = \"new\"\n"
            "[Weapon2]\n"
            "Base = \"Weapon1\"\n"
            "Name = \"Big Rifle\"\n"), "an overlay parses");
        int nChanged = -1, nAdded = -1;
        char err[256];
        Check(b.Merge(o, &nChanged, &nAdded, err, sizeof(err)) && nChanged == 1 && nAdded == 1,
              "the overlay merges: one tag changed, one added");
        Check(b.GetType("Weapon0", "Damage") == CButeMgr::IntType && b.GetInt("Weapon0", "Damage", 0) == 13,
              "a number keeps the type retail gives the key: 12.6 over an int is the int 13");
        Check(b.GetDouble("Weapon0", "Speed", 0.0) == 2.5 && !strcmp(b.GetString("Weapon0", "Name", ""), "Pistol"),
              "a key the overlay leaves alone keeps retail's value");
        Check(!strcmp(b.GetString("Weapon0", "Extra", ""), "new"), "a key the overlay adds is added");
        Check(b.Exist("Weapon2") && !strcmp(b.GetString("Weapon2", "Name", ""), "Big Rifle") &&
              b.GetInt("Weapon2", "Damage", 0) == 30 && b.GetDouble("Weapon2", "Speed", 0.0) == 1.5,
              "a new tag starts as a copy of its Base, and the overlay's keys replace the copy's");
        Check(!b.Exist("Weapon2", "Base"), "Base is the overlay's own key: the new tag has none");
        Check(!strcmp(b.GetString("Weapon1", "Name", ""), "Rifle") && b.GetInt("Weapon1", "Damage", 0) == 30,
              "the Base tag itself is unchanged");
    }
    struct Bad { const char* label; const char* overlay; const char* expect; };
    const Bad bad[] =
    {
        { "a Base for a tag that exists",  "[Weapon0]\nBase = \"Weapon1\"\nDamage = 99\n", "for a tag that exists" },
        { "a Base naming no tag",          "[Weapon0]\nDamage = 99\n[Weapon9]\nBase = \"Nothing\"\n", "names no tag" },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        CButeMgr b, o;
        ParseText(b, kRetail);
        ParseText(o, bad[i].overlay);
        char err[256];
        const bool bOk = b.Merge(o, NULL, NULL, err, sizeof(err));
        Check(!bOk && strstr(err, bad[i].expect) && b.GetInt("Weapon0", "Damage", 0) == 10 && !b.Exist("Weapon9"),
              std::string(bad[i].label) + " merges nothing and says why: " + err);
    }
    printf("\npassed %d, failed %d\n", g_nPass, g_nFail);
    return g_nFail ? 1 : 0;
}

bool EndsWithTxt(const std::string& name)
{
    if (name.size() < 4)
        return false;
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext)
        c = (char)tolower((unsigned char)c);
    return ext == ".txt";
}

// Every retail file merged onto a second parse of itself: every tag counts as changed, none as
// added, and every value reads back as it was
int Retail(const char* pDir)
{
    std::error_code ec;
    std::filesystem::directory_iterator it(pDir, ec);
    if (ec) { printf("no folder %s\n", pDir); return 2; }
    int nFiles = 0, nTags = 0, nBad = 0, nSkipped = 0;
    for (const std::filesystem::directory_entry& e : it)
    {
        if (!e.is_regular_file())
            continue;
        const std::string name = e.path().filename().string();
        if (!EndsWithTxt(name)) continue;
        std::ifstream f(e.path(), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        const std::string text = ss.str();
        CButeMgr b, o, ref;
        // NOLF2's Animations*.txt are another parser's format and are never overlaid
        if (!ParseText(b, text) || !ParseText(o, text) || !ParseText(ref, text)) { printf("  %s: not a ButeMgr file, skipped\n", name.c_str()); ++nSkipped; continue; }
        int nChanged = 0, nAdded = 0;
        char err[256];
        if (!b.Merge(o, &nChanged, &nAdded, err, sizeof(err)) || nAdded != 0 || nChanged != ref.GetNumTagsInFileOrder())
        {
            printf("  %s: merge %d changed %d added (%s)\n", name.c_str(), nChanged, nAdded, err);
            ++nBad;
            continue;
        }
        for (int t = 0; t < ref.GetNumTagsInFileOrder(); ++t)
        {
            const char* tag = ref.GetTagInFileOrder(t);
            struct Keys { std::vector<std::string> k; } keys;
            ref.GetKeys(tag, [](const char* k, CButeMgr::CSymTabItem*, void* p) { ((Keys*)p)->k.push_back(k); return true; }, &keys);
            for (size_t i = 0; i < keys.k.size(); ++i)
            {
                const char* key = keys.k[i].c_str();
                if (ref.GetType(tag, key) != b.GetType(tag, key)) { printf("  %s [%s] %s: type changed\n", name.c_str(), tag, key); ++nBad; }
                else if (ref.GetType(tag, key) == CButeMgr::StringType && strcmp(ref.GetString(tag, key, ""), b.GetString(tag, key, "")))
                { printf("  %s [%s] %s: string changed\n", name.c_str(), tag, key); ++nBad; }
                else if (ref.GetType(tag, key) == CButeMgr::DoubleType && ref.GetDouble(tag, key, 0.0) != b.GetDouble(tag, key, 0.0))
                { printf("  %s [%s] %s: number changed\n", name.c_str(), tag, key); ++nBad; }
                else if (ref.GetType(tag, key) == CButeMgr::IntType && ref.GetInt(tag, key, 0) != b.GetInt(tag, key, 0))
                { printf("  %s [%s] %s: number changed\n", name.c_str(), tag, key); ++nBad; }
            }
        }
        ++nFiles;
        nTags += ref.GetNumTagsInFileOrder();
    }
    printf("%d files, %d tags merged onto themselves; %d differences; %d not ButeMgr files\n", nFiles, nTags, nBad, nSkipped);
    return nBad || !nFiles ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest")) return Selftest();
    if (argc == 3 && !strcmp(argv[1], "--retail")) return Retail(argv[2]);
    fprintf(stderr, "usage: testButeMerge --selftest | --retail <dir>\n");
    return 2;
}
