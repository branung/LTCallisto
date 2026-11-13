// testLT1Mod: libs/LT1Mod's checks
//
//     testLT1Mod --selftest          every check below; exit 0 if all pass
//     testLT1Mod --sig <root> [game] the signature of the tree at <root>, as
//                                    a game would compute it with <root>
//                                    mounted (default game "Shogo")
//
// --sig is for holding the signature against tools/lt1mod_sig.py, an independent implementation,
// on the committed fixture, since two machines must agree on it.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"
#include "lt1mod_textfile.h"
#include "lt1mod_classdefs.h"
#include "lt1mod_script.h"
#include "ltserverobj.h"

namespace fs = std::filesystem;

namespace
{

int g_nPass = 0;
int g_nFail = 0;

void Check(bool cond, const char* what)
{
    if (cond)
    {
        ++g_nPass;
        printf("  ok    %s\n", what);
    }
    else
    {
        ++g_nFail;
        printf("  FAIL  %s\n", what);
    }
}

std::string Lower(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

// A tree in memory, answering the way the engine does: case insensitive lookups, '\\' paths, one
// directory level per listing, full paths in the listing. The listing order is reversed on
// request, to prove nothing depends on it.
class MemSource : public LT1Mod::FileSource
{
public:
    std::map<std::string, std::pair<std::string, std::string> > files;  // lower -> (spelling, data)
    bool bReverse;

    MemSource() : bReverse(false) {}

    void Put(const std::string& path, const std::string& data) { files[Lower(path)] = std::make_pair(path, data); }

    virtual bool ReadFile(const char* path, std::string& out)
    {
        std::map<std::string, std::pair<std::string, std::string> >::iterator it = files.find(Lower(path));
        if (it == files.end())
            return false;
        out = it->second.second;
        return true;
    }

    virtual void ListDir(const char* dir, std::vector<LT1Mod::DirEntry>& out)
    {
        out.clear();
        std::string prefix = Lower(dir) + "\\";
        std::map<std::string, bool> seen;
        for (std::map<std::string, std::pair<std::string, std::string> >::iterator it = files.begin();
             it != files.end(); ++it)
        {
            if (it->first.compare(0, prefix.size(), prefix) != 0)
                continue;
            std::string rest = it->second.first.substr(prefix.size());
            size_t slash = rest.find('\\');
            std::string name = slash == std::string::npos ? rest : rest.substr(0, slash);
            std::string full = std::string(dir) + "\\" + name;
            if (seen.count(Lower(full)))
                continue;
            seen[Lower(full)] = true;
            LT1Mod::DirEntry e;
            e.path = full;
            e.isDir = slash != std::string::npos;
            out.push_back(e);
        }
        if (bReverse)
            std::reverse(out.begin(), out.end());
    }
};

// A directory on disk as the root of the mounted trees. Paths arrive '\\' separated and are
// matched case insensitively per component, as the engine matches inside a rez.
class DiskSource : public LT1Mod::FileSource
{
public:
    explicit DiskSource(const fs::path& root) : m_root(root) {}

    virtual bool ReadFile(const char* path, std::string& out)
    {
        fs::path p;
        if (!Resolve(path, p) || !fs::is_regular_file(p))
            return false;
        std::ifstream f(p, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return (bool)f || f.eof();
    }

    virtual void ListDir(const char* dir, std::vector<LT1Mod::DirEntry>& out)
    {
        out.clear();
        fs::path p;
        if (!Resolve(dir, p) || !fs::is_directory(p))
            return;
        for (fs::directory_iterator it(p); it != fs::directory_iterator(); ++it)
        {
            LT1Mod::DirEntry e;
            e.path = std::string(dir) + "\\" + it->path().filename().string();
            e.isDir = it->is_directory();
            out.push_back(e);
        }
    }

private:
    bool Resolve(const char* path, fs::path& out)
    {
        fs::path cur = m_root;
        std::string s(path);
        size_t start = 0;
        while (start <= s.size())
        {
            size_t end = s.find_first_of("\\/", start);
            std::string part = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!part.empty())
            {
                bool bFound = false;
                if (fs::is_directory(cur))
                {
                    for (fs::directory_iterator it(cur); it != fs::directory_iterator(); ++it)
                    {
                        if (Lower(it->path().filename().string()) == Lower(part))
                        {
                            cur = it->path();
                            bFound = true;
                            break;
                        }
                    }
                }
                if (!bFound)
                    return false;
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        out = cur;
        return true;
    }

    fs::path m_root;
};

const char* const kGoodManifest =
    "// A comment, as mod authors will write them.\n"
    "[Mod]\n"
    "Id      = \"TestMod\"\n"
    "Game    = \"shogo\"\n"
    "Version = \"0.1\"\n";

void TestManifest()
{
    printf("manifest\n");
    LT1Mod::State s;

    {
        MemSource src;
        src.Put("Worlds\\Level.dat", "not a mod");
        LT1Mod::Load(src, "Shogo", s);
        Check(!s.present && !s.valid, "no Mod\\Mod.txt: not present");
        Check(LT1Mod::Signature(s) == "none", "no mod: signature \"none\"");
        Check(LT1Mod::Describe(s) == "no mod", "no mod: described as \"no mod\"");
    }
    {
        MemSource src;
        src.Put("Mod\\Weapons.txt", "[KatoBurst]\n");
        LT1Mod::Load(src, "Shogo", s);
        Check(!s.present, "Mod\\ files without Mod.txt: not present");
    }
    {
        MemSource src;
        src.Put("MOD\\MOD.TXT", kGoodManifest);         // a rez's spelling
        LT1Mod::Load(src, "Shogo", s);
        Check(s.present && s.valid, "valid manifest, upper-case path: valid");
        Check(s.manifest.id == "TestMod" && s.manifest.version == "0.1", "Id and Version read");
        Check(s.manifest.name == "TestMod", "Name defaults to Id");
        Check(s.manifest.game == "shogo", "Game matched case-insensitively");
        Check(s.fileCount == 1, "one file hashed");
    }

    struct Bad { const char* text; const char* game; const char* needle; const char* what; };
    const Bad bad[] = {
        { "[Mod]\nGame = \"Shogo\"\n",                     "Shogo",  "has no Id",      "missing Id" },
        { "[Mod]\nId = \"X\"\n",                           "Shogo",  "has no Game",    "missing Game" },
        { "[Mod]\nId = LOF\nGame = \"Shogo\"\n",           "Shogo",  "",               "unquoted Id" },
        { "[Mod]\nId = \"has space\"\nGame = \"Shogo\"\n", "Shogo",  "must be 1-31",   "Id with a space" },
        { "[Mod]\nId = \"../up\"\nGame = \"Shogo\"\n",     "Shogo",  "must be 1-31",   "Id with a path" },
        { "[Mod]\nId = \"abcdefghijabcdefghijabcdefghijab\"\nGame = \"Shogo\"\n", "Shogo", "must be 1-31", "32-character Id" },
        { "[Mod]\nId = \"X\"\nGame = \"Blood2\"\n",        "Shogo",  "is for Blood2",  "another game's mod" },
        { "[Other]\nId = \"X\"\n",                         "Shogo",  "no [Mod]",       "no [Mod] section" },
        { "[Mod]\nId = \"X\"\n[Mod]\nGame = \"Shogo\"\n",  "Shogo",  "Duplicate tag",  "repeated [Mod] (ButeMgr's rule)" },
        { "[Mod]\nId = \"X\nGame = \"Shogo\"\n",           "Shogo",  "",               "unterminated string" },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        MemSource src;
        src.Put("Mod\\Mod.txt", bad[i].text);
        LT1Mod::Load(src, bad[i].game, s);
        bool bOk = s.present && !s.valid && !s.error.empty() &&
                   s.error.find(bad[i].needle) != std::string::npos &&
                   LT1Mod::Signature(s).compare(0, 8, "invalid:") == 0;
        std::string what = std::string("rejected: ") + bad[i].what;
        Check(bOk, what.c_str());
        if (!bOk)
            printf("        error was: %s\n", s.error.c_str());
    }
}

void TestHash()
{
    printf("hash\n");
    LT1Mod::State a, b;

    MemSource base;
    base.Put("Mod\\Mod.txt", kGoodManifest);
    base.Put("Mod\\Weapons.txt", "[KatoBurst]\nDamage = 125\n");
    base.Put("Mod\\Sub\\Deep\\Strings.txt", "[1]\nText = \"x\"\n");
    base.Put("Worlds\\Level.dat", "outside the mod");
    LT1Mod::Load(base, "Shogo", a);
    Check(a.valid && a.fileCount == 3, "three files under Mod\\, subdirectories walked");

    {
        MemSource m = base;
        m.bReverse = true;
        LT1Mod::Load(m, "Shogo", b);
        Check(a.hash == b.hash, "listing order does not change the hash");
    }
    {
        MemSource m;
        m.Put("MOD\\MOD.TXT", kGoodManifest);
        m.Put("mod\\WEAPONS.TXT", "[KatoBurst]\nDamage = 125\n");
        m.Put("Mod\\sub\\DEEP\\strings.txt", "[1]\nText = \"x\"\n");
        LT1Mod::Load(m, "Shogo", b);
        Check(a.hash == b.hash, "file-name case does not change the hash");
    }
    {
        MemSource m = base;
        m.Put("Worlds\\Level.dat", "a different level");
        m.Put("Models\\New.abc", "new");
        LT1Mod::Load(m, "Shogo", b);
        Check(a.hash == b.hash, "files outside Mod\\ do not change the hash");
    }
    {
        MemSource m = base;
        m.Put("Mod\\Weapons.txt", "[KatoBurst]\nDamage = 126\n");
        LT1Mod::Load(m, "Shogo", b);
        Check(a.hash != b.hash, "one changed byte changes the hash");
    }
    {
        MemSource m = base;
        m.Put("Mod\\Extra.txt", "");
        LT1Mod::Load(m, "Shogo", b);
        Check(a.hash != b.hash && b.fileCount == 4, "an added empty file changes the hash");
    }
    {
        // Two files whose concatenation is the same must not collide: the
        // length field is what separates them.
        MemSource m1, m2;
        m1.Put("Mod\\Mod.txt", kGoodManifest);
        m1.Put("Mod\\a", "xy");
        m1.Put("Mod\\b", "");
        m2.Put("Mod\\Mod.txt", kGoodManifest);
        m2.Put("Mod\\a", "x");
        m2.Put("Mod\\b", "y");
        LT1Mod::Load(m1, "Shogo", a);
        LT1Mod::Load(m2, "Shogo", b);
        Check(a.hash != b.hash, "moving a byte between files changes the hash");
    }
    {
        // The known answer, computed independently (tools/lt1mod_sig.py, and by hand from FNV-1a's
        // definition): a tree holding only Mod\Mod.txt = "[Mod]\nId = \"K\"\nGame = \"Shogo\"\n".
        MemSource m;
        m.Put("Mod\\Mod.txt", "[Mod]\nId = \"K\"\nGame = \"Shogo\"\n");
        LT1Mod::Load(m, "Shogo", a);
        std::string sig = LT1Mod::Signature(a);
        printf("        known-answer signature: %s\n", sig.c_str());
        Check(sig == "K:418d350ed64859dd", "known-answer signature");
    }
}

void TestAttributes()
{
    printf("attributes\n");
    // Fifty tags declared in descending order. A hash table will not give them back in this order
    // on either compiler; the file order must.
    std::string text;
    std::vector<std::string> want;
    for (int i = 49; i >= 0; --i)
    {
        char tag[32];
        snprintf(tag, sizeof(tag), "Entry%02d", i);
        want.push_back(tag);
        text += std::string("[") + tag + "]\nN = " + std::to_string(i) + "\n";
    }
    text += "[Typed]\nI = 7\nF = 0.5\nB = TRUE\nS = \"str\"\nV = <1.0, 2.0, 3.0>\nDup = 1\nDup = 2\n";
    want.push_back("Typed");

    LT1Mod::Attributes a;
    Check(a.Parse(text.data(), text.size(), "test.txt"), "parses");
    Check(a.Tags() == want, "tags come back in file order");
    Check(a.Has("typed") && a.Has("TYPED", "i") && !a.Has("Typed", "Missing") && !a.Has("Nope"),
          "Has is case-insensitive and exact about absence");
    Check(a.GetInt("Typed", "I", 0) == 7, "GetInt");
    Check(a.GetFloat("Typed", "F", 0) == 0.5f && a.GetFloat("Typed", "I", 0) == 7.0f,
          "GetFloat, and an int read as a float");
    Check(a.GetBool("Typed", "B", false), "GetBool");
    Check(a.GetString("Typed", "S", "") == "str", "GetString");
    float v[3] = { 0, 0, 0 };
    Check(a.GetVector("Typed", "V", v) && v[0] == 1.0f && v[1] == 2.0f && v[2] == 3.0f, "GetVector");
    Check(a.GetInt("Typed", "Dup", 0) == 1, "a repeated key keeps its first value (ButeMgr's rule)");
    Check(a.GetInt("Typed", "Missing", 42) == 42 && a.Error().empty(), "absent key gives the default, no error");
    Check(a.GetInt("Typed", "S", 42) == 42 && a.Error().find("is not an integer") != std::string::npos,
          "wrong type gives the default and says so");
    std::vector<std::string> keys = a.Keys("Typed");
    Check(keys.size() == 6 && keys.front() == "B" && keys.back() == "V", "Keys, sorted");

    // ButeMgr scans every token into a fixed 4096 byte buffer with no bound, so a long enough
    // value in a mod's file wrote past it. Parse refuses such a file before ButeMgr sees it.
    for (int nLen = 5000; nLen <= 50000; nLen *= 10)
    {
        const std::string longs = "[A]\nK = \"" + std::string((size_t)nLen, 'x') + "\"\n";
        const std::string longw = "[A]\nK" + std::string((size_t)nLen, 'k') + " = 1\n";
        LT1Mod::Attributes big;
        const bool bS = big.Parse(longs.data(), longs.size(), "long.txt");
        const std::string eS = big.Error();
        const bool bW = big.Parse(longw.data(), longw.size(), "long.txt");
        const std::string eW = big.Error();
        std::string comment = "// ";
        for (int i = 0; i < nLen / 5; ++i) comment += "abcd ";
        const std::string longc = comment + "\n[A]\nK = 1\n";
        const bool bC = big.Parse(longc.data(), longc.size(), "long.txt");
        Check(!bC && big.Error().find("line 1 is longer than 4090") != std::string::npos,
              (std::string("a ") + std::to_string(comment.size()) + "-character comment line with spaces is refused too "
               "(ButeMgr buffers comments)").c_str());
        Check(!bS && !bW && eS.find("longer than 4090") != std::string::npos && eW.find("longer than 4090") != std::string::npos,
              (std::string("a ") + std::to_string(nLen) + "-character string or word is refused before ButeMgr's "
               "4096-byte token buffer (" + eS + ")").c_str());
    }
    {
        const std::string ok = "[A]\nK = \"" + std::string(4000, 'x') + "\"\n";
        LT1Mod::Attributes fits;
        Check(fits.Parse(ok.data(), ok.size(), "fits.txt") && fits.GetString("A", "K", "").size() == 4000,
              "a 4000-character string still parses whole");
    }

    LT1Mod::Attributes bad;
    const char dup[] = "[A]\nX = 1\n[a]\nX = 2\n";
    Check(!bad.Parse(dup, sizeof(dup) - 1, "dup.txt") && bad.Tags().empty() &&
          bad.Error().find("dup.txt") != std::string::npos,
          "a repeated tag (any case) fails the parse, naming the file");

    // Reparsing the same object starts clean.
    const char small[] = "[Only]\nX = 1\n";
    Check(a.Parse(small, sizeof(small) - 1, "small.txt") && a.Tags().size() == 1 && !a.Has("Typed"),
          "a second Parse replaces the first");
}

struct Weapon
{
    float damage;
    int   clip;
};

void ApplyWeapon(Weapon& w, const LT1Mod::Attributes& a, const char* tag, bool)
{
    if (a.Has(tag, "Damage")) w.damage = a.GetFloat(tag, "Damage", w.damage);
    if (a.Has(tag, "Clip"))   w.clip   = a.GetInt(tag, "Clip", w.clip);
}

void TestRegistry()
{
    printf("registry\n");
    LT1Mod::Registry r(4);
    Check(r.AddBase("Pulse") == 0 && r.AddBase("UNUSED1") == 1, "base ids are dense, in call order");
    Check(r.AddBase("pulse") == -1, "a base name cannot repeat, in any case");
    Check(r.Append("Late") == -1, "Append before Freeze is refused");
    r.Freeze();
    Check(r.AddBase("TooLate") == -1, "AddBase after Freeze is refused");
    std::string err;
    Check(r.Append("Kato", &err) == 2 && r.Append("Mort", &err) == 3, "appended ids follow the base");
    Check(r.Append("Full", &err) == -1 && err.find("full") != std::string::npos, "a full registry refuses, saying so");
    Check(r.Find("KATO") == 2 && r.Find("nope") == -1, "Find is case-insensitive");
    Check(r.NumBase() == 2 && r.IsBase(1) && !r.IsBase(2), "base count survives appends");

    LT1Mod::Table<Weapon> t(256);
    Weapon pulse = { 20.0f, 30 };
    Weapon shotgun = { 15.0f, 8 };
    t.AddBase("PulseRifle", pulse);
    t.AddBase("Shotgun", shotgun);
    t.Freeze();

    const char ov[] =
        "[Shotgun]\nDamage = 17.5\n"            // override one field
        "[ZNew]\nDamage = 40\nClip = 12\n"      // append; Z sorts last ...
        "[ANew]\nClip = 3\n";                   // ... and A first, but file order wins
    LT1Mod::Attributes a;
    a.Parse(ov, sizeof(ov) - 1, "weapons.txt");
    Weapon def = { 1.0f, 1 };
    Check(t.Apply(a, def, ApplyWeapon, &err), "overlay applies");
    Check(t[0].damage == 20.0f && t[0].clip == 30, "an entry the overlay does not name is untouched");
    Check(t[1].damage == 17.5f && t[1].clip == 8, "an override changes only the keys it gives");
    Check(t.Names().Find("ZNew") == 2 && t.Names().Find("ANew") == 3, "new entries numbered in file order");
    Check(t[2].damage == 40.0f && t[2].clip == 12 && t[3].damage == 1.0f && t[3].clip == 3,
          "new entries start from the default and take their keys");
}

void TestSavePath()
{
    printf("save paths\n");
    LT1Mod::SetActive(LT1Mod::State());
    Check(LT1Mod::SavePath("Save\\Quick.sav") == "Save\\Quick.sav" && LT1Mod::SaveDir().empty(),
          "no mod: save paths unchanged");

    MemSource src;
    src.Put("Mod\\Mod.txt", kGoodManifest);
    LT1Mod::State s;
    LT1Mod::Load(src, "Shogo", s);
    LT1Mod::SetActive(s);
    Check(LT1Mod::IsActive(), "a valid mod becomes active");
    Check(LT1Mod::SavePath("Save\\Quick.sav") == "Save\\TestMod\\Quick.sav", "Save\\ gains the mod's Id");
    Check(LT1Mod::SavePath("save/Slot1.sav") == "Save\\TestMod\\Slot1.sav", "any case, either separator");
    Check(LT1Mod::SavePath("Saved\\x") == "Saved\\x" && LT1Mod::SavePath("players\\a.b2c") == "players\\a.b2c",
          "paths outside Save\\ unchanged");
    Check(LT1Mod::SaveDir() == "Save\\TestMod", "SaveDir");
    Check(LT1Mod::SavePath(LT1Mod::SavePath("Save\\Quick.sav").c_str()) == "Save\\TestMod\\Quick.sav" &&
          LT1Mod::SavePath("Save\\testmod\\Quick.sav") == "Save\\TestMod\\Quick.sav",
          "mapping twice is mapping once");
    Check(LT1Mod::SavePath("Save\\TestModX.sav") == "Save\\TestMod\\TestModX.sav",
          "a file whose name starts with the Id is still mapped");

    MemSource broken;
    broken.Put("Mod\\Mod.txt", "[Mod]\n");
    LT1Mod::Load(broken, "Shogo", s);
    LT1Mod::SetActive(s);
    Check(!LT1Mod::IsActive() && LT1Mod::SavePath("Save\\Quick.sav") == "Save\\Quick.sav",
          "an invalid mod is not active and moves no saves");
    LT1Mod::SetActive(LT1Mod::State());
}

// lt1mod_textfile.h: fgets over the engine's copy of a file, as fopen "r" reads it. The oracle is
// the C library's own fgets on a real file holding the LF form of the text, at every buffer size
// from 2 to 300. That holds for a CR LF file too, because the LF form is what a Windows text mode
// stream hands fgets.
void TestTextLines()
{
    printf("text lines\n");
    std::string longLine(700, 'x');
    const char* const kTexts[] =
    {
        "one\ntwo\n\nfour\n",
        "no newline at the end",
        "\n\n\n",
        "",
        "a line, then a long one\n",
    };
    std::vector<std::string> texts(kTexts, kTexts + sizeof(kTexts) / sizeof(kTexts[0]));
    texts.push_back(longLine + "\nshort\n" + longLine);
    int nCases = 0, nBad = 0;
    std::string first;
    // The oracle files hold no CR before an LF, so a Windows "r" changes nothing in them
    const std::string sPath = (std::filesystem::temp_directory_path() / "lt1mod_textlines.txt").string();
    const char* path = sPath.c_str();
    for (size_t t = 0; t < texts.size(); ++t)
        for (int crlf = 0; crlf < 3; ++crlf)
        {
            // 0: the text as is; 1: every LF as CR LF; 2: lone CRs added.
            std::string lf = texts[t], given;
            if (crlf == 2) { for (size_t i = 0; i < lf.size(); ++i) if (lf[i] == ' ') lf[i] = '\r'; }
            for (size_t i = 0; i < lf.size(); ++i) { if (crlf == 1 && lf[i] == '\n') given += '\r'; given += lf[i]; }
            FILE* f = fopen(path, "wb");
            fwrite(lf.data(), 1, lf.size(), f);
            fclose(f);
            for (int n = 2; n <= 300; ++n)
            {
                std::string want, got;
                FILE* g = fopen(path, "r");
                char a[400], b[400];
                while (fgets(a, n, g)) want += std::string(a) + "|";
                fclose(g);
                LT1Mod::TextLines tl(given);
                while (tl.Gets(b, n)) got += std::string(b) + "|";
                ++nCases;
                if (want != got)
                {
                    if (!nBad) first = "text " + std::to_string(t) + " form " + std::to_string(crlf) + " size " + std::to_string(n);
                    ++nBad;
                }
            }
        }
    remove(path);
    Check(nBad == 0, (nBad ? std::to_string(nBad) + " of " + std::to_string(nCases) + " differ from fgets, first " + first
                           : std::string("TextLines::Gets is fgets on ") + std::to_string(nCases) +
                             " files and buffer sizes: LF files, CR LF files read as text mode reads them, lone CRs kept").c_str());
    LT1Mod::TextLines bent(std::string("a\r\nb\r\n"));
    char c[16];
    bent.Gets(c, 16);
    Check(std::string(c) == "a\n", "calibration: a CR LF line comes back as text mode returns it, \"a\\n\", not \"a\\r\\n\"");
}

// A mod's archetypes as class names: copies of their host's ClassDef, named, with the host as
// parent
void DummyConstruct(void*) {}
void TestClassDefs()
{
    printf("classdefs\n");
    static char props[64];    // stands for a PropDef list: only its address is compared
    static ClassDef base, host, other;
    memset(&base, 0, sizeof(base)); memset(&host, 0, sizeof(host)); memset(&other, 0, sizeof(other));
    base.m_ClassName = "BaseClass";
    host.m_ClassName = "ModAI"; host.m_ParentClass = &base; host.m_ConstructFn = DummyConstruct;
    host.m_DestructFn = DummyConstruct; host.m_nProps = 1; host.m_Props = (PropDef*)props; host.m_ClassObjectSize = 1234;
    host.m_ClassFlags = 7; host.m_pInternal[0] = &host;
    other.m_ClassName = "Trooper";
    ClassDef* native[] = { &base, &host, &other };

    std::vector<LT1Mod::ClassName> names;
    names.push_back(LT1Mod::ClassName("DemoTrooper", "ModAI"));
    names.push_back(LT1Mod::ClassName("trooper", "ModAI"));        // a native class, in another case
    names.push_back(LT1Mod::ClassName("Ghost", "NoSuchHost"));
    names.push_back(LT1Mod::ClassName("DEMOTROOPER", "ModAI"));    // one already made
    names.push_back(LT1Mod::ClassName("Sniper", "modai"));         // the host without case
    names.push_back(LT1Mod::ClassName("Third", "ModAI"));          // past the room
    ClassDef* out[2] = { 0, 0 };
    std::string sAdded, sSkipped;
    const int n = LT1Mod::AppendClasses(names, native, 3, out, 2, sAdded, sSkipped);
    Check(n == 2 && sAdded == "DemoTrooper (ModAI), Sniper (modai)",
          "an archetype whose host is a native class becomes a class, the host matched without case");
    Check(sSkipped == "trooper (a class has the name), Ghost (no such host class), DEMOTROOPER (a class has the name), "
                      "Third (no room)",
          "a native class's name, an unknown host, a name already made and no room are each skipped, with why");
    const ClassDef* d = n ? out[0] : 0;
    Check(d && !strcmp(d->m_ClassName, "DemoTrooper") && d->m_ParentClass == &host && !d->m_ConstructFn &&
          !d->m_DestructFn && !d->m_PluginFn && d->m_nProps == 0 && !d->m_Props,
          "the class is its host's child with no functions or properties of its own, so the engine gives it the host's");
    Check(d && d->m_ClassObjectSize == 1234 && d->m_ClassFlags == 7 && !d->m_pInternal[0] && !d->m_pInternal[1],
          "it keeps the host's object size and flags, and none of the engine's per-class data");
    Check(n == 2 && out[1]->m_ParentClass == &host && host.m_nProps == 1 && host.m_ConstructFn == DummyConstruct,
          "and the host itself is unchanged");
}

void Check(bool cond, const std::string& what) { Check(cond, what.c_str()); }

// A mod's Mod\Script.lua, against a host that records what the script asked of the game
struct RecHost : public LT1Mod::ScriptHost
{
    std::vector<std::string> lines, commands;
    float fHealth;
    double fTime;
    RecHost() : fHealth(40.0f), fTime(12.5) {}
    virtual void Print(const char* p) { lines.push_back(p); }
    virtual void Command(const char* p) { commands.push_back(p); }
    virtual double Time() { return fTime; }
    virtual bool GetHealth(void*, float& f) { f = fHealth; return true; }
    virtual bool SetHealth(void*, float f) { fHealth = f; return true; }
    virtual bool GetPos(void*, float& x, float& y, float& z) { x = 1; y = 2; z = 3; return true; }
    // Version 2, recorded.
    virtual bool GetMaxHealth(void*, float& f) { f = 100.0f; return true; }
    virtual bool Damage(void*, float n) { commands.push_back("damage " + std::to_string((int)n)); return true; }
    virtual bool SendTrigger(void*, const char* m) { commands.push_back(std::string("send ") + m); return true; }
    virtual bool Teleport(void*, float x, float, float) { commands.push_back("teleport " + std::to_string((int)x)); return true; }
    virtual bool Remove(void*) { commands.push_back("remove"); return true; }
    virtual int  RandomInt(int lo, int) { return lo; }
    virtual bool Hud(void* h, const char* t) { commands.push_back(std::string(h ? "hud one " : "hud all ") + t); return true; }
    // Version 5: what it would send to a client.
    virtual bool SendToClient(void* h, const char* n, const char* pl)
    {
        commands.push_back(std::string(h ? "client one " : "client all ") + n + " " + pl);
        return true;
    }
    // Version 4: as the engine does, the object is created (raising OnCreate) before Spawn
    // returns it
    int spawned[4];
    int nSpawned = 0;
    virtual void* Spawn(const char* a, float x, float, float, const char* props, std::string& why)
    {
        if (strcmp(a, "A") != 0) { why = "no such archetype"; return NULL; }
        if (nSpawned >= 4) { why = "full"; return NULL; }
        void* h = &spawned[nSpawned++];
        commands.push_back("spawn " + std::string(a) + " " + std::to_string((int)x) + " " + props);
        LT1Mod::ScriptObject o = { h, "spawned1", a, a };
        LT1Mod::Script_Create(o);
        return h;
    }
    // Version 6: "Prop" is a class the script does not hold (true); the yaw is recorded. Anything
    // else is Spawn's.
    int prop = 0;
    virtual void* SpawnEx(const char* a, float x, float y, float z, const char* props, float yaw, bool& bObject,
                          std::string& why)
    {
        if (strcmp(a, "Prop") != 0) { bObject = true; return Spawn(a, x, y, z, props, why); }
        bObject = false;
        commands.push_back("spawn Prop yaw " + std::to_string((int)yaw));
        return &prop;
    }
    virtual bool Explode(float x, float, float, float r, float lo, float hi, const char* fx, void* src, std::string& why)
    {
        if (strcmp(fx, "BOGUS") == 0) { why = "no explosion \"BOGUS\""; return false; }
        commands.push_back("explode " + std::to_string((int)x) + " r" + std::to_string((int)r) + " " +
                           std::to_string((int)lo) + "-" + std::to_string((int)hi) + " " + fx + (src ? " from" : ""));
        return true;
    }
    bool Has(const std::string& s) const
    {
        for (size_t i = 0; i < lines.size(); ++i) if (lines[i].find(s) != std::string::npos) return true;
        return false;
    }
};

bool LoadScript(const std::string& text, RecHost& host, std::string& err)
{
    MemSource src;
    src.Put("Mod\\Mod.txt", "[Mod]\nId = \"S\"\nGame = \"Shogo\"\n");
    if (!text.empty()) src.Put("Mod\\Script.lua", text);
    LT1Mod::State st;
    LT1Mod::Load(src, "Shogo", st);
    LT1Mod::SetActive(st);
    return LT1Mod::Script_Load(src, &host, err);
}

void TestScript()
{
    printf("script\n");
    std::string err;
    RecHost h0;
    Check(LoadScript("", h0, err) && err.empty() && !LT1Mod::Script_Loaded(), "a mod without Script.lua has no script, and no error");

    const char* kScript =
        "kept = nil\n"
        "function OnWorldStart(w) print('world', w, mod.world, mod.api, mod.time())\n"
        "  if kept then print('kept valid', kept:valid()); kept:health() end end\n"
        "function OnCreate(o) kept = o; print(o:name(), o:archetype(), o:class(), o:health(), o:pos())\n"
        "  o:set_health(5) end\n"
        "function OnDamage(o, n) print('damage', n) end\n"
        "function OnDeath(o) mod.command('MSG door1 TRIGGER') end\n"
        "function OnDamaged() end\n";
    RecHost h;
    Check(LoadScript(kScript, h, err) && LT1Mod::Script_Loaded(), "a script loads" + (err.empty() ? std::string() : ": " + err));
    Check(LT1Mod::Script_Summary() == "handles OnWorldStart, OnCreate, OnDamage, OnDeath\n"
                                      "WARNING: OnDamaged is not an event, so nothing calls it",
          "its summary names the events it handles and warns of an On... that is none: " + LT1Mod::Script_Summary());

    int obj = 0;
    LT1Mod::ScriptObject o = { &obj, "trooper1", "DemoTrooper", "DemoTrooper" };
    LT1Mod::Script_Create(o);
    Check(h.Has("Script: trooper1\tDemoTrooper\tDemoTrooper\t40.0\t1.0\t2.0\t3.0") && h.fHealth == 5.0f,
          "OnCreate gets the object: name, archetype, class, health and position, and sets its health");
    LT1Mod::Script_Damage(o, 7.5f);
    LT1Mod::Script_Death(o);
    Check(h.Has("Script: damage\t7.5") && h.commands.size() == 1 && h.commands[0] == "MSG door1 TRIGGER",
          "OnDamage gets the amount, and OnDeath runs a command through the game");
    h.lines.clear();
    LT1Mod::Script_WorldStart("01_Ambush");
    Check(h.Has("Script: world\t01_Ambush\t01_Ambush\t" + std::to_string(LT1Mod::kScriptApi) + "\t12.5") && h.Has("Script: kept valid\ttrue") &&
          !h.Has("is off"),
          "OnWorldStart gets the world (mod.world, mod.api, mod.time), and an object kept from an earlier event is "
          "still valid and usable (57.5)");
    h.lines.clear();
    LT1Mod::Script_Removed(&obj);
    LT1Mod::Script_WorldStart("02_X");
    Check(h.Has("Script: world\t02_X") && h.Has("Script: kept valid\tfalse") && h.Has("the object is gone") &&
          h.Has("OnWorldStart is off until the next world"),
          "once the game removes it, the kept object is invalid, using it is an error, and the handler is off");
    h.lines.clear();
    LT1Mod::Script_WorldStart("03_Y");
    Check(h.Has("Script: world\t03_Y"), "and on again at the next world");

    struct Bad { const char* label; const char* text; const char* expect; };
    const Bad bad[] =
    {
        { "a syntax error",                     "function (", "Script.lua:1:" },
        { "an error at the top level",          "error('boom')", "boom" },
        { "a loop past the budget",             "while true do end", "ran past its budget" },
        { "memory past the cap",                "local t = {} for i = 1, 1e9 do t[i] = string.rep('x', 1000) .. i end", "not enough memory" },
        { "a precompiled chunk",                "\x1bLua", "attempt to load a binary chunk" },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        RecHost hb;
        err.clear();
        const bool bLoaded = LoadScript(bad[i].text, hb, err);
        Check(!bLoaded && !LT1Mod::Script_Loaded() && err.find(bad[i].expect) != std::string::npos,
              std::string(bad[i].label) + " loads nothing and says why: " + err.substr(0, err.find('\n')));
    }
    RecHost hc;
    const bool bCmdLoaded = LoadScript("function OnCommand(name, ...) print('cmd', name, select('#', ...), ...) end", hc, err);
    const std::string sCmdSum = LT1Mod::Script_Summary();
    Check(bCmdLoaded && sCmdSum == "handles OnCommand", "a script with OnCommand loads: " + sCmdSum);
    const char* cargs[] = { "3", "two words" };
    Check(LT1Mod::Script_Command("open_vault", cargs, 2) && hc.Has("Script: cmd\topen_vault\t2\t3\ttwo words"),
          "SCRIPT's name and its arguments reach OnCommand as strings");
    RecHost hn;
    LoadScript("x = 1", hn, err);
    Check(!LT1Mod::Script_Command("open_vault", cargs, 2), "a script without OnCommand answers no SCRIPT");
    RecHost hs;
    Check(LoadScript("print(io, os, type(require), dofile, loadfile, load, debug, package, string.rep('a', 3))", hs, err) &&
          hs.Has("Script: nil\tnil\tfunction\tnil\tnil\tnil\tnil\tnil\taaa"),
          "the sandbox has no io, os, dofile, loadfile, load, debug or package, its own require (API 7), and the "
          "string library");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

// Version 2: queries, references, removal, timers, OnUpdate, the game's random draws, messages,
// players, world end
void TestScriptV2()
{
    printf("script, version 2\n");
    const char* kScript =
        "kept = {} seen = {}\n"
        "function OnCreate(o) kept[#kept + 1] = o; seen[o] = true end\n"
        "function OnRemove(o) print('removed', o:name(), o:archetype(), o:valid()) end\n"
        "function OnTrigger(o, m, from) print('trigger', o:name(), m, from) end\n"
        "function OnPlayerJoin(p) print('join', p:name(), p:is_player(), #mod.players()) end\n"
        "function OnPlayerLeave(p) print('leave', p:name()) end\n"
        "function OnWorldEnd(w) print('end', w, #mod.objects()) end\n"
        "ups = 0\n"
        "function OnUpdate(dt) ups = ups + 1 end\n"
        "function OnCommand(n, a)\n"
        "  if n == 'find' then local f = mod.find('t1'); print('find', f:name(), f == kept[1], seen[f], mod.find('nobody')) end\n"
        "  if n == 'list' then print('list', #mod.objects('A'), #mod.objects(), #mod.players()) end\n"
        "  if n == 'gone' then print('gone', kept[1]:valid(), pcall(kept[1].health, kept[1])) end\n"
        "  if n == 'timers' then mod.after(1, function() print('after') end); t = mod.every(0.5, function() print('every') end) end\n"
        "  if n == 'cancel' then t:cancel() end\n"
        "  if n == 'rand' then print('rand', mod.random(1, 6), math.random(3, 9), math.randomseed) end\n"
        "  if n == 'acts' then local o = kept[2]; o:damage(25); o:send('OPEN'); o:teleport(64, 0, 0); o:remove();\n"
        "    print('max', o:max_health()); mod.hud(nil, 'hello') end\n"
        "  if n == 'rate' then mod.set_update_rate(2) end\n"
        "  if n == 'badrate' then mod.set_update_rate(50) end\n"
        "  if n == 'ups' then print('ups', ups) end\n"
        "end\n";
    RecHost h;
    std::string err;
    h.fTime = 0.0;
    Check(LoadScript(kScript, h, err), "a version 2 script loads" + (err.empty() ? std::string() : ": " + err));
    LT1Mod::Script_WorldStart("W1");
    int a = 0, b = 0, p = 0;
    LT1Mod::ScriptObject oa = { &a, "t1", "A", "ModAI" }, ob = { &b, "t2", "B", "ModAI" }, op = { &p, "Player1", "", "CPlayerObj" };
    LT1Mod::Script_Create(oa);
    LT1Mod::Script_Create(ob);
    auto Cmd = [](const char* n) { LT1Mod::Script_Command(n, NULL, 0); };
    Cmd("find");
    Check(h.Has("Script: find\tt1\ttrue\ttrue\tnil"),
          "mod.find returns the object OnCreate was handed, equal to it and the same table key; an unknown name is nil");
    LT1Mod::Script_PlayerJoin(op);
    Cmd("list");
    Check(h.Has("Script: join\tPlayer1\ttrue\t1") && h.Has("Script: list\t1\t2\t1"),
          "mod.objects lists by archetype and in all, mod.players the players, and a player is_player");
    LT1Mod::Script_Trigger(oa, "OPEN", "door1");
    Check(h.Has("Script: trigger\tt1\tOPEN\tdoor1"), "OnTrigger gets the object, the message and the sender's name");
    Cmd("acts");
    Check(h.commands.size() >= 5 && h.commands[0] == "damage 25" && h.commands[1] == "send OPEN" &&
          h.commands[2] == "teleport 64" && h.commands[3] == "remove" && h.commands[4] == "hud all hello" && h.Has("Script: max\t100.0"),
          "damage, send, teleport, remove, max_health and hud reach the game, in order");
    LT1Mod::Script_Removed(&a);
    Cmd("gone");
    Check(h.Has("Script: removed\tt1\tA\tfalse") && h.Has("Script: gone\tfalse\tfalse"),
          "OnRemove reads the name and archetype of an object going (valid() false), and afterwards a kept "
          "reference is invalid and using it fails");
    h.lines.clear();
    Cmd("rand");
    Check(h.Has("Script: rand\t1\t3\tnil"), "mod.random and math.random are the game's draws, and math.randomseed is gone");
    h.lines.clear();
    Cmd("timers");
    h.fTime = 0.6; LT1Mod::Script_Update();
    const bool bOne = h.Has("Script: every") && !h.Has("Script: after");
    h.lines.clear();
    h.fTime = 1.05; LT1Mod::Script_Update();
    const bool bTwo = h.Has("Script: every") && h.Has("Script: after");
    Cmd("cancel");
    h.lines.clear();
    h.fTime = 3.0; LT1Mod::Script_Update();
    Check(bOne && bTwo && !h.Has("Script: every") && !h.Has("Script: after"),
          "mod.every fires each period, mod.after once when due, and a cancelled timer never again");
    Cmd("rate");
    for (int i = 1; i <= 20; ++i) { h.fTime = 3.0 + i * 0.1; LT1Mod::Script_Update(); }
    h.lines.clear();
    Cmd("ups");
    Check(h.Has("Script: ups\t4"), "OnUpdate runs at the rate set (2 a second over 2 seconds of frames): " +
          (h.lines.empty() ? std::string("nothing") : h.lines[0]));
    h.lines.clear();
    Cmd("badrate");
    Check(h.Has("set_update_rate wants 0 to 10"), "and refuses a rate past 10");
    LT1Mod::Script_PlayerLeave(op);
    h.lines.clear();
    LT1Mod::Script_WorldEnd();
    Check(h.Has("Script: end\tW1\t1"), "OnWorldEnd gets the world while its objects still live");
    h.lines.clear();
    LT1Mod::Script_WorldStart("W2");
    Cmd("list");
    Check(h.Has("Script: list\t0\t0\t0"), "and the next world starts with no objects from the last");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

// Version 3, what lasts: mod.save across worlds and through a save, the timers by name, a fresh
// state per world, OnRestore
void TestScriptSaves()
{
    printf("script, saves\n");
    const char* kScript =
        "mod.require_api(3)\n"
        "function ring() print('ring', mod.save.n) end\n"
        "function OnCreate(o) if o:name() == 't1' then mod.save.boss = o end end\n"
        "function OnWorldStart(w) print('start', w, leaked, mod.save.n, mod.save.deep and mod.save.deep.list[2]) end\n"
        "function OnRestore(w) print('restore', w, mod.save.n, mod.save.boss == mod.find('t1'), mod.save.boss ~= nil) end\n"
        "function OnCommand(n)\n"
        "  if n == 'set' then leaked = 'yes'; mod.save.n = (mod.save.n or 0) + 1;\n"
        "    mod.save.deep = { list = { 'a', 'b' }, flag = true, x = 2.5 } end\n"
        "  if n == 'timer' then mod.after(5, ring); mod.after(5, function() end) end\n"
        "  if n == 'bad' then mod.save.f = print end\n"
        "  if n == 'unbad' then mod.save.f = nil end\n"
        "end\n";
    RecHost h;
    std::string err;
    h.fTime = 0.0;
    Check(LoadScript(kScript, h, err) && LT1Mod::Script_Present(), "a version 3 script loads" + (err.empty() ? std::string() : ": " + err));
    LT1Mod::Script_WorldStart("W1");
    LT1Mod::Script_Command("set", NULL, 0);
    LT1Mod::Script_WorldEnd();
    h.lines.clear();
    LT1Mod::Script_WorldStart("W2");
    Check(h.Has("Script: start\tW2\tnil\t1\tb"),
          "mod.save crosses a world change, nested tables and all, and a global from the last world does not (a fresh state): " +
          (h.lines.empty() ? std::string("nothing") : h.lines.back()));
    int a = 0;
    LT1Mod::ScriptObject oa = { &a, "t1", "A", "ModAI" };
    LT1Mod::Script_Create(oa);
    LT1Mod::Script_Command("timer", NULL, 0);
    h.lines.clear();
    h.fTime = 1.0;
    const std::string blob = LT1Mod::Script_SaveBlob();
    Check(blob.compare(0, 4, "LTS3") == 0 && h.Has("timer") && h.Has("is not a global, so it is not saved"),
          "a save holds mod.save and the named timer, and says the anonymous one is not saved");
    LT1Mod::Script_Command("bad", NULL, 0);
    h.lines.clear();
    LT1Mod::Script_SaveBlob();
    Check(h.Has("mod.save is not saved: a function cannot be saved"), "a function in mod.save is refused, by name of its type");
    LT1Mod::Script_Command("unbad", NULL, 0);

    // The restore: a new game session loads the script, the world's objects
    // come back adopted, then the save's blob, then OnRestore.
    RecHost h2;
    h2.fTime = 50.0;
    Check(LoadScript(kScript, h2, err), "the script loads again for the restored game");
    int a2 = 0;
    LT1Mod::ScriptObject oa2 = { &a2, "t1", "A", "ModAI" };
    LT1Mod::Script_Adopt(oa2);
    LT1Mod::Script_RestoreBlob(blob);
    LT1Mod::Script_WorldStart("W2", true);
    Check(h2.Has("Script: restore\tW2\t1\ttrue\ttrue") && !h2.Has("Script: start"),
          "OnRestore, not OnWorldStart, with mod.save as saved and its object reference found again by name and archetype: " +
          (h2.lines.empty() ? std::string("nothing") : h2.lines.back()));
    h2.lines.clear();
    h2.fTime = 53.5; LT1Mod::Script_Update();
    const bool bEarly = h2.Has("Script: ring");
    h2.fTime = 54.5; LT1Mod::Script_Update();
    Check(!bEarly && h2.Has("Script: ring\t1"), "the named timer comes back with the time it had left (4 of 5 seconds)");

    RecHost h3;
    const std::string sNext = std::to_string(LT1Mod::kScriptApi + 1);
    Check(!LoadScript("mod.require_api(" + sNext + ")", h3, err) &&
          err.find("needs mod.api " + sNext + "; this game has " + std::to_string(LT1Mod::kScriptApi)) != std::string::npos,
          "a script needing a later API than the game's is refused with both numbers: " + err.substr(0, err.find('\n')));
    LT1Mod::Script_Clear();
    Check(!LT1Mod::Script_Present() && LT1Mod::Script_SaveBlob().empty(), "with no script, a save has no script field");
    LT1Mod::SetActive(LT1Mod::State());
}

// Version 4: mod.spawn, and ScriptTrigger's call
void TestScriptSpawn()
{
    printf("script, spawning and ScriptTrigger\n");
    const char* kScript =
        "made = nil\n"
        "function OnCreate(o) made = o end\n"
        "function poke(msg, from) print('poked', msg, from) end\n"
        "function broken() error('no') end\n"
        "function OnCommand(n)\n"
        "  if n == 'spawn' then local s = mod.spawn('A', 64, 0, 0, 'Name spawned1');\n"
        "    print('spawn', s:name(), s == made, s == mod.find('spawned1'), #mod.objects('A')) end\n"
        "  if n == 'bad' then print('bad', pcall(mod.spawn, 'Nobody', 0, 0, 0)) end\n"
        "end\n";
    RecHost h;
    std::string err;
    Check(LoadScript(kScript, h, err), "a version 4 script loads" + (err.empty() ? std::string() : ": " + err));
    LT1Mod::Script_WorldStart("W");
    LT1Mod::Script_Command("spawn", NULL, 0);
    Check(h.Has("Script: spawn\tspawned1\ttrue\ttrue\t1") && h.commands.size() == 1 && h.commands[0] == "spawn A 64 Name spawned1",
          "mod.spawn makes the archetype at the point with the props, and returns the object OnCreate was handed");
    LT1Mod::Script_Command("bad", NULL, 0);
    Check(h.Has("Script: bad\tfalse\t") && h.Has("spawn Nobody: no such archetype"),
          "a spawn the game refuses is a script error naming the archetype and why");
    const char* args[] = { "OPEN", "lever1" };
    Check(LT1Mod::Script_Function("poke", args, 2) && h.Has("Script: poked\tOPEN\tlever1"),
          "a ScriptTrigger's function is called with the message and the sender: " +
          (h.lines.empty() ? std::string("nothing") : h.lines.back()));
    Check(!LT1Mod::Script_Function("nothere", args, 2), "and a function the script lacks answers false, for the game to say so");
    h.lines.clear();
    LT1Mod::Script_Function("broken", args, 2);
    const bool bOff = h.Has("broken is off until the next world");
    h.lines.clear();
    LT1Mod::Script_Function("broken", args, 2);
    Check(bOff && h.lines.empty(), "a function that errors is off until the next world, as a handler is");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

// Version 6: the events' new arguments, mod.spawn of a class the script does not hold, and
// mod.explode
void TestScriptV6()
{
    printf("script, version 6\n");
    const char* kScript =
        "mod.require_api(6)\n"
        "boss = nil\n"
        "function OnCreate(o, placed) print('create', o:name(), placed) if o:name() == 'boss1' then boss = o end end\n"
        "function OnDamage(o, n, from, kind) print('damage', n, from and from:name(), kind) end\n"
        "function OnDeath(o, killer, kind, dmg) print('death', killer and killer:name(), kind, dmg) end\n"
        "function OnCommand(n)\n"
        "  if n == 'prop' then print('prop', mod.spawn('Prop', 0, 0, 0, '', 90)) end\n"
        "  if n == 'boom' then mod.explode(5, 0, 0, 450, 25, 50, 'DIVINE_SHOCKBALL', boss) end\n"
        "  if n == 'bad' then print('bad', pcall(mod.explode, 0, 0, 0, 0, 1, 2))\n"
        "    print('bad2', pcall(mod.explode, 0, 0, 0, 10, 1, 2, 'BOGUS')) end\n"
        "end\n";
    RecHost h;
    std::string err;
    Check(LoadScript(kScript, h, err), "a version 6 script loads" + (err.empty() ? std::string() : ": " + err));
    LT1Mod::Script_WorldStart("W");
    int a = 0, b = 0, stranger = 0;
    LT1Mod::ScriptObject oa = { &a, "boss1", "", "Prophet" }, ob = { &b, "grunt1", "", "CultistAI" };
    LT1Mod::Script_Create(oa, true);
    LT1Mod::Script_Create(ob);
    Check(h.Has("Script: create\tboss1\ttrue") && h.Has("Script: create\tgrunt1\tfalse"),
          "OnCreate says whether the level placed the object, and a game that does not say passes false");
    LT1Mod::Script_Damage(ob, 4.0f, &a, "FIRE");
    LT1Mod::Script_Damage(ob, 2.0f, &stranger, NULL);
    Check(h.Has("Script: damage\t4.0\tboss1\tFIRE") && h.Has("Script: damage\t2.0\tnil\tnil"),
          "OnDamage gets the attacker and the damage type; an attacker the script never saw, and no type, are nil");
    LT1Mod::Script_Death(ob, &a, "LEECH", 75.0f);
    LT1Mod::Script_Death(ob, &stranger);
    Check(h.Has("Script: death\tboss1\tLEECH\t75.0") && h.Has("Script: death\tnil\tnil\tnil"),
          "OnDeath gets the killer the script knows, the type and the killing blow; else nils");
    LT1Mod::Script_Command("prop", NULL, 0);
    Check(h.Has("Script: prop\ttrue") && !h.commands.empty() && h.commands.back() == "spawn Prop yaw 90",
          "mod.spawn of a class the script does not hold makes it, turned by the yaw, and answers true");
    LT1Mod::Script_Command("boom", NULL, 0);
    Check(!h.commands.empty() && h.commands.back() == "explode 5 r450 25-50 DIVINE_SHOCKBALL from",
          "mod.explode passes the point, radius, damage range, effect and source to the game: " +
          (h.commands.empty() ? std::string("nothing") : h.commands.back()));
    LT1Mod::Script_Command("bad", NULL, 0);
    Check(h.Has("Script: bad\tfalse\t") && h.Has("radius above 0") && h.Has("Script: bad2\tfalse\t") &&
          h.Has("explode: no explosion \"BOGUS\""),
          "a radius of 0 is a script error, and so is an effect the game refuses, with its reason");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());

    // A version 5 game's host: SpawnEx falls back to Spawn, mod.explode cannot.
    struct OldHost : public RecHost
    {
        virtual void* SpawnEx(const char* a, float x, float y, float z, const char* p, float yaw, bool& o, std::string& w)
        { return LT1Mod::ScriptHost::SpawnEx(a, x, y, z, p, yaw, o, w); }
        virtual bool Explode(float x, float y, float z, float r, float lo, float hi, const char* fx, void* s, std::string& w)
        { return LT1Mod::ScriptHost::Explode(x, y, z, r, lo, hi, fx, s, w); }
    } old;
    Check(LoadScript("function OnCommand(n) print('old', pcall(mod.explode, 0, 0, 0, 10, 1, 2))\n"
                     "  print('oldspawn', mod.spawn('A', 1, 0, 0) ~= true) end", old, err),
          "a script loads on a host written before version 6");
    LT1Mod::Script_Command("x", NULL, 0);
    Check(old.Has("Script: old\tfalse\t") && old.Has("this game has no explosions for a script") &&
          old.Has("Script: oldspawn\ttrue"),
          "there mod.explode is a script error saying the game has none, and mod.spawn still answers the archetype's object");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

// The client's script, against a host that records what it drew
struct RecClientHost : public LT1Mod::ScriptClientHost
{
    std::vector<std::string> lines, drawn;
    double fTime;
    RecClientHost() : fTime(3.0) {}
    virtual void Print(const char* p) { lines.push_back(p); }
    virtual double Time() { return fTime; }
    virtual bool DrawHudText(int x, int y, const char* t, int r, int g, int b)
    {
        char s[64];
        snprintf(s, sizeof(s), "text %d %d %d,%d,%d ", x, y, r, g, b);
        drawn.push_back(s + std::string(t));
        return true;
    }
    virtual bool DrawRect(int x, int y, int w, int h, int r, int g, int b)
    {
        char s[64];
        snprintf(s, sizeof(s), "rect %d %d %d %d %d,%d,%d", x, y, w, h, r, g, b);
        drawn.push_back(s);
        return true;
    }
    virtual bool Sound2D(const char* p) { drawn.push_back(std::string("sound ") + p); return true; }
    std::vector<std::string> sent;
    virtual bool SendToServer(const char* n, const char* pl) { sent.push_back(std::string(n) + " " + pl); return true; }
    bool Has(const std::string& s) const
    {
        for (size_t i = 0; i < lines.size(); ++i) if (lines[i].find(s) != std::string::npos) return true;
        return false;
    }
};

bool LoadClientScript(const std::string& text, RecClientHost& host, std::string& err, bool bServerToo = false)
{
    MemSource src;
    src.Put("Mod\\Mod.txt", "[Mod]\nId = \"S\"\nGame = \"Shogo\"\n");
    if (!text.empty()) src.Put("Mod\\ClientScript.lua", text);
    if (bServerToo) src.Put("Mod\\Script.lua", "function OnWorldStart() end");
    LT1Mod::State st;
    LT1Mod::Load(src, "Shogo", st);
    LT1Mod::SetActive(st);
    return LT1Mod::Script_Load(src, &host, err, LT1Mod::kScriptClient);
}

// Modules (API 7; lt1mod_script.h, MODULES)
bool LoadWithModules(const std::string& main, const std::vector<std::pair<std::string, std::string> >& mods,
                     RecHost& host, std::string& err, bool bClient = false, RecClientHost* pClient = NULL)
{
    MemSource src;
    src.Put("Mod\\Mod.txt", "[Mod]\nId = \"S\"\nGame = \"Shogo\"\n");
    src.Put(bClient ? "Mod\\ClientScript.lua" : "Mod\\Script.lua", main);
    for (size_t i = 0; i < mods.size(); ++i) src.Put(mods[i].first, mods[i].second);
    LT1Mod::State st;
    LT1Mod::Load(src, "Shogo", st);
    LT1Mod::SetActive(st);
    if (bClient) return LT1Mod::Script_Load(src, pClient, err, LT1Mod::kScriptClient);
    return LT1Mod::Script_Load(src, &host, err);
}

void TestScriptModules()
{
    printf("script, modules (version 7)\n");
    typedef std::vector<std::pair<std::string, std::string> > Mods;
    std::string err;
    const Mods kMods =
    {
        { "Mod\\Scripts\\ec\\revenants.lua",
          "runs = (runs or 0) + 1\n"
          "local M = {}\n"
          "function M.on_death(name) print('blast', name) end\n"
          "return M\n" },
        { "Mod\\Scripts\\util.lua", "local u = require('ec.revenants') return { twice = function(x) return x * 2 end, rev = u }\n" },
    };
    RecHost h;
    const char* kMain =
        "mod.require_api(7)\n"
        "local rev = require('ec.revenants')\n"
        "local util = require 'util'\n"
        "print('same', rev == require('ec.revenants'), util.rev == rev, runs, util.twice(21), mod.api)\n"
        "function OnCommand(n) if n == 'later' then print('later', pcall(require, 'never')) end\n"
        "  if n == 'again' then print('again', require('util') == util) end rev.on_death(n) end\n";
    Check(LoadWithModules(kMain, kMods, h, err) && h.Has("Script: same\ttrue\ttrue\t1\t42\t7"),
          "require runs Mod\\Scripts\\<dots as folders>.lua once, a second require (from the script or another "
          "module) answers the same value, and mod.api is 7" + (err.empty() ? std::string() : ": " + err));
    Check(LT1Mod::Script_Summary() == "handles OnCommand; modules ec.revenants, util",
          "the summary names the modules in the order they loaded: " + LT1Mod::Script_Summary());
    const char* none[] = { NULL };
    LT1Mod::Script_Command("again", none, 0);
    LT1Mod::Script_Command("later", none, 0);
    Check(h.Has("Script: again\ttrue") && h.Has("Script: blast\tagain") &&
          h.Has("modules are read while the script loads"),
          "in an event a loaded module still answers, and one never read is an error that says when modules are read");
    // A world's rebuild runs the modules again, from the text read at load.
    LT1Mod::Script_WorldEnd();
    LT1Mod::Script_Command("again", none, 0);
    Check(LT1Mod::Script_Loaded() && h.Has("Script: same\ttrue\ttrue\t1\t42\t7") &&
          h.lines.back() == "Script: blast\tagain",
          "after a world's rebuild the script and its modules run again, with nothing re-read");
    LT1Mod::Script_Clear();

    struct Bad { const char* label; const char* main; Mods mods; const char* expect; };
    const Bad bad[] =
    {
        { "a path in the name", "require('../Mod')", {}, "a module's name is letters, digits and _ joined by dots" },
        { "two dots", "require('a..b')", {}, "a module's name is letters, digits and _ joined by dots" },
        { "a slash", "require('a/b')", {}, "a module's name is letters, digits and _ joined by dots" },
        { "a module the mod has not", "require('missing')", {}, "no Mod\\Scripts\\missing.lua in the mod" },
        { "a cycle", "require('a')", { { "Mod\\Scripts\\a.lua", "require('b')" }, { "Mod\\Scripts\\b.lua", "require('a')" } },
          "require(\"a\"): a cycle" },
        { "an error in a module", "require('bad')", { { "Mod\\Scripts\\bad.lua", "local x = 1\nerror('broken')" } },
          "Mod\\Scripts\\bad.lua:2: broken" },
        { "a syntax error in a module", "require('syn')", { { "Mod\\Scripts\\syn.lua", "function (" } },
          "Mod\\Scripts\\syn.lua:1:" },
        { "bytecode", "require('bin')", { { "Mod\\Scripts\\bin.lua", std::string("\x1bLua") } },
          "attempt to load a binary chunk" },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        RecHost hb;
        err.clear();
        const bool bLoaded = LoadWithModules(bad[i].main, bad[i].mods, hb, err);
        Check(!bLoaded && !LT1Mod::Script_Loaded() && err.find(bad[i].expect) != std::string::npos,
              std::string(bad[i].label) + " loads nothing and says why: " + err.substr(0, err.find('\n')));
        LT1Mod::Script_Clear();
    }

    // The client's script requires the same file, in a state of its own.
    RecHost hs;
    RecClientHost hc;
    Check(LoadWithModules("local r = require('ec.revenants') print('client', type(r.on_death), runs)", kMods, hs, err,
                          true, &hc) && hc.Has("ClientScript: client\tfunction\t1"),
          "ClientScript.lua requires the same module, and runs its own copy" + (err.empty() ? std::string() : ": " + err));
    LT1Mod::Script_Clear();

    // The checker lints each module under its own file's name.
    {
        MemSource src;
        src.Put("Mod\\Mod.txt", "[Mod]\nId = \"S\"\nGame = \"Shogo\"\n");
        src.Put("Mod\\Script.lua", "mod.require_api(7)\nrequire('m')\n");
        src.Put("Mod\\Scripts\\m.lua", "local function f() mod.spwan('x') end\nreturn {}\n");
        LT1Mod::State st;
        LT1Mod::Load(src, "Shogo", st);
        LT1Mod::SetActive(st);
        static std::vector<std::string> printed;
        printed.clear();
        LT1Mod::Script_Check(src, LT1Mod::kScriptServer, [](const char* p) { printed.push_back(p); });
        std::string all;
        for (size_t i = 0; i < printed.size(); ++i) all += printed[i] + "\n";
        Check(all.find("Mod\\Scripts\\m.lua:1: WARNING: mod.spwan is not in the API") != std::string::npos &&
              all.find("Mod\\Script.lua: needs mod.api 7 (require, line 2)") != std::string::npos,
              "the checker lints a module under its own name, and counts require as version 7: " + all);
        LT1Mod::Script_Clear();
    }
    LT1Mod::SetActive(LT1Mod::State());
}


void TestClientScript()
{
    printf("client script\n");
    std::string err;
    RecClientHost h0;
    Check(LoadClientScript("", h0, err, true) && err.empty() && !LT1Mod::Script_Loaded(),
          "a mod with only a server Script.lua has no client script: the client reads ClientScript.lua alone");

    const char* kScript =
        "count = 0\n"
        "function OnClientStart() print('start', mod.api, mod.save, mod.command, mod.find, mod.spawn, mod.hud ~= nil) end\n"
        "function OnWorldStart(w) print('world', w, mod.world) mod.after(1, function() print('rang') end) end\n"
        "function OnDraw(w, h) count = count + 1 mod.hud.rect(0, h - 20, w, 20, 0, 0, 64)\n"
        "  mod.hud.text(4, h - 18, 'Kills: ' .. count) mod.hud.text(4, 4, 'red', 255, 0, 0) end\n"
        "function OnCommand(n, a) print('cmd', n, a) if n == 'early' then mod.hud.text(0, 0, 'x') end\n"
        "  if n == 'beep' then mod.sound('Sounds\\\\beep.wav') end end\n"
        "function OnCreate() end\n";
    RecClientHost h;
    Check(LoadClientScript(kScript, h, err) && LT1Mod::Script_Loaded(), "a client script loads" + (err.empty() ? std::string() : ": " + err));
    Check(LT1Mod::Script_Summary() == "handles OnClientStart, OnWorldStart, OnDraw, OnCommand\n"
                                      "WARNING: OnCreate is an event of Mod\\Script.lua, not of this file, so nothing here calls it",
          "its summary names the client's events, and says a server event is the server file's: " + LT1Mod::Script_Summary());
    LT1Mod::Script_ClientStart();
    Check(h.Has("ClientScript: start\t" + std::to_string(LT1Mod::kScriptApi) + "\tnil\tnil\tnil\tnil\ttrue"),
          "OnClientStart runs; the client has mod.hud, and no mod.save, mod.command, mod.find or mod.spawn (L14, 57.11); "
          "its lines say they are the client's");
    LT1Mod::Script_WorldStart("01_Ambush");
    Check(h.Has("ClientScript: world\t01_Ambush\t01_Ambush"), "OnWorldStart gets the world, and so does mod.world");
    h.fTime = 4.5;
    LT1Mod::Script_Update();
    Check(h.Has("ClientScript: rang"), "a client timer rings on the client's clock");

    LT1Mod::Script_Draw(640, 480);
    LT1Mod::Script_Draw(640, 480);
    Check(h.drawn.size() == 6 && h.drawn[0] == "rect 0 460 640 20 0,0,64" && h.drawn[1] == "text 4 462 255,255,255 Kills: 1" &&
          h.drawn[2] == "text 4 4 255,0,0 red" && h.drawn[4] == "text 4 462 255,255,255 Kills: 2",
          "OnDraw is given the screen, and draws rectangles and text, white unless coloured, every frame: " +
          (h.drawn.empty() ? std::string("nothing") : h.drawn[0]));
    const char* args[] = { "x" };
    h.drawn.clear();
    Check(LT1Mod::Script_Command("early", args, 1) && h.Has("ClientScript: cmd\tearly\tx") &&
          h.Has("mod.hud.text draws only inside OnDraw") && h.drawn.empty(),
          "the `script` console command reaches OnCommand, and drawing outside OnDraw is an error that draws nothing");
    LT1Mod::Script_WorldStart("02_X");
    LT1Mod::Script_Command("beep", NULL, 0);
    Check(h.drawn.size() == 1 && h.drawn[0] == "sound Sounds\\beep.wav", "mod.sound plays a local sound");

    RecClientHost hb;
    Check(LoadClientScript("function OnDraw() while true do end end", hb, err), "a client script with a runaway OnDraw loads");
    LT1Mod::Script_Draw(640, 480);
    const bool bOff = hb.Has("ran past its budget") && hb.Has("OnDraw is off until the next world");
    hb.lines.clear();
    LT1Mod::Script_Draw(640, 480);
    Check(bOff && hb.lines.empty(), "an OnDraw past its per-frame budget (L12) is off for the world, and says so once");
    LT1Mod::Script_WorldStart("W");
    hb.lines.clear();
    LT1Mod::Script_Draw(640, 480);
    Check(hb.Has("ran past its budget"), "and runs again in the next world");

    RecClientHost hs;
    Check(LoadClientScript("print(io, os, type(require), load, debug, mod.players, mod.objects)", hs, err) &&
          hs.Has("ClientScript: nil\tnil\tfunction\tnil\tnil\tnil\tnil"),
          "the client's sandbox is the server's, without the game's objects");
    LT1Mod::Script_Clear();

    // The server's side is unchanged by a client load before it.
    RecHost hsv;
    Check(LoadScript("function OnDraw() end print(mod.save ~= nil, mod.hud == nil)", hsv, err) && hsv.Has("Script: true\tfalse") &&
          LT1Mod::Script_Summary() == "no event handlers\nWARNING: OnDraw is an event of Mod\\ClientScript.lua, not of this file, so nothing here calls it",
          "a server script loaded after a client one is the server's again: mod.save, its mod.hud, its events");
    LT1Mod::Script_Draw(640, 480);
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

// Version 5's messages: each side's sends and events, the size cap and the rate
void TestScriptMessages()
{
    printf("script messages\n");
    std::string err;
    RecHost h;
    const char* kServer =
        "function OnClientMessage(p, n, pl) print('from', p:name(), p:is_player(), p == mod.players()[1], n, pl)\n"
        "  if n == 'hello' then print('sent', mod.client(p, 'ack', pl), mod.client(nil, 'all', 'x')) end end\n"
        "function OnCommand(n)\n"
        "  if n == 'big' then print('big', pcall(mod.client, nil, 'n', string.rep('x', 1025))) end\n"
        "  if n == 'max' then print('max', mod.client(nil, string.rep('n', 64), string.rep('x', 1024))) end\n"
        "  if n == 'noname' then print('noname', pcall(mod.client, nil, '')) end\n"
        "  if n == 'nul' then print('nul', pcall(mod.client, nil, 'n', 'a\\0b')) end\n"
        "end\n";
    Check(LoadScript(kServer, h, err) && LT1Mod::Script_Summary() == "handles OnCommand, OnClientMessage",
          "a server script with OnClientMessage loads: " + LT1Mod::Script_Summary() + err);
    int p1 = 0, p2 = 0;
    LT1Mod::ScriptObject o1 = { &p1, "Sanjuro", "", "CPlayerObj" }, o2 = { &p2, "Kura", "", "CPlayerObj" };
    LT1Mod::Script_PlayerJoin(o1);
    Check(LT1Mod::Script_ClientMessage(o1, "hello", "two words") &&
          h.Has("Script: from\tSanjuro\ttrue\ttrue\thello\ttwo words") && h.Has("Script: sent\ttrue\ttrue") &&
          h.commands.size() == 2 && h.commands[0] == "client one ack two words" && h.commands[1] == "client all all x",
          "a client's message reaches OnClientMessage with the player (the same object mod.players has), the name and "
          "the payload; mod.client answers that player, and nil every player");
    LT1Mod::ScriptObject o1late = { &p1, "Sanjuro2", "", "CPlayerObj" };
    h.lines.clear();
    LT1Mod::Script_ClientMessage(o1late, "rename", "");
    Check(h.Has("Script: from\tSanjuro2\ttrue\ttrue\trename"),
          "a player's name is the game's latest, on the same object: Blood 2 names a player after it joins");
    LT1Mod::Script_Command("max", NULL, 0);
    LT1Mod::Script_Command("big", NULL, 0);
    LT1Mod::Script_Command("noname", NULL, 0);
    LT1Mod::Script_Command("nul", NULL, 0);
    Check(h.Has("Script: max\ttrue") && h.Has("Script: big\tfalse") && h.Has("longer than 1024 bytes") &&
          h.Has("Script: noname\tfalse") && h.Has("wants a name") && h.Has("Script: nul\tfalse") && h.Has("NUL byte"),
          "a 64-byte name and a 1024-byte payload go; one byte more, no name, or a NUL is an error where it is sent");

    // The rate, per player, by the game's clock.
    h.lines.clear();
    h.fTime += 1.0;                     // a full allowance again after the hello
    int nIn = 0;
    for (int i = 0; i < 25; ++i) nIn += LT1Mod::Script_ClientMessage(o1, "spam", "") ? 1 : 0;
    int nWarn = 0;
    for (size_t i = 0; i < h.lines.size(); ++i) if (h.lines[i].find("dropped so far") != std::string::npos) ++nWarn;
    int nOther = 0;
    for (int i = 0; i < 20; ++i) nOther += LT1Mod::Script_ClientMessage(o2, "spam", "") ? 1 : 0;
    Check(nIn == 20 && LT1Mod::Script_MessagesDropped() == 5 && nWarn == 1 && h.Has("player Sanjuro sent more than 20") && nOther == 20,
          "25 messages from one player at once: 20 delivered, 5 dropped, counted and said once; another player has "
          "an allowance of its own: " + std::to_string(nIn) + " in, " + std::to_string(LT1Mod::Script_MessagesDropped()) + " dropped");
    h.fTime += 0.5;
    int nLater = 0;
    for (int i = 0; i < 15; ++i) nLater += LT1Mod::Script_ClientMessage(o1, "spam", "") ? 1 : 0;
    Check(nLater == 10, "half a second later the allowance has 10 again: " + std::to_string(nLater));
    const std::string big(1025, 'x');
    const int nBefore = LT1Mod::Script_MessagesDropped();
    h.fTime += 1.0;
    Check(!LT1Mod::Script_ClientMessage(o1, "n", big.c_str()) && !LT1Mod::Script_ClientMessage(o1, "", "") &&
          LT1Mod::Script_MessagesDropped() == nBefore + 2 && h.Has("a message was dropped"),
          "a malformed message from a client, which a modified one can send, is dropped and counted where it arrives");
    LT1Mod::Script_Clear();

    // The client's side.
    RecClientHost c;
    const char* kClient =
        "function OnServerMessage(n, pl) print('got', n, pl) end\n"
        "function OnCommand(n)\n"
        "  if n == 'send' then print('send', mod.server('hello', 'two words'), mod.server('bare')) end\n"
        "  if n == 'spam' then local ok = 0 for i = 1, 25 do if mod.server('spam') then ok = ok + 1 end end print('spam', ok) end\n"
        "  if n == 'big' then print('big', pcall(mod.server, 'n', string.rep('x', 1025))) end\n"
        "end\n";
    Check(LoadClientScript(kClient, c, err) && LT1Mod::Script_Summary() == "handles OnCommand, OnServerMessage",
          "a client script with OnServerMessage loads: " + LT1Mod::Script_Summary() + err);
    LT1Mod::Script_Command("send", NULL, 0);
    Check(c.Has("ClientScript: send\ttrue\ttrue") && c.sent.size() == 2 && c.sent[0] == "hello two words" && c.sent[1] == "bare ",
          "mod.server sends the name and payload to the server, the payload empty when left out");
    c.sent.clear();
    LT1Mod::Script_Command("spam", NULL, 0);
    Check(c.Has("ClientScript: spam\t18") && c.sent.size() == 18 && c.Has("this client sent more than 20"),
          "the client keeps to the rate itself: of 25 at once, the 18 its allowance had left (2 went just before) are "
          "sent, mod.server answers false for the rest, and it says so");
    LT1Mod::Script_Command("big", NULL, 0);
    Check(c.Has("ClientScript: big\tfalse") && c.Has("longer than 1024 bytes"), "an over-size payload is an error where it is sent");
    Check(LT1Mod::Script_ServerMessage("score", "7") && c.Has("ClientScript: got\tscore\t7"),
          "a server's message reaches OnServerMessage with its name and payload");
    Check(!LT1Mod::Script_ServerMessage("n", big.c_str()) && !LT1Mod::Script_ClientMessage(o1, "n", "p"),
          "an over-size one from the server is dropped, and the client has no OnClientMessage to raise");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

std::string Joined(const std::vector<std::string>& v)
{
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? " | " : "") + v[i];
    return s;
}

// Diagnostics: the static pass, the trace, and the slow event warning
void TestScriptDiagnostics()
{
    printf("script diagnostics\n");
    const char* kServer =
        "mod.require_api(3)\n"                                   // 1
        "function OnCreate(o) mod.spawn('A', 0, 0, 0) end\n"     // 2: spawn is 4
        "function OnClientMessage(p, n) end\n"                   // 3: an event of 5
        "function OnCommand() mod.hud.text(0, 0, 'x') end\n"     // 4: the client's
        "local x = mod.fnid\n"                                   // 5: no such call
        "-- mod.bogus in a comment\n"                            // 6
        "local s = 'mod.bogus2' .. \"mod.bogus3\"\n"            // 7
        "--[[ mod.bogus4\n"                                      // 8
        "  across lines ]] local y = [==[ mod.bogus5\n"          // 9
        "]==] local z = mod.fnid2\n";                            // 10
    const std::string lint = LT1Mod::Script_Lint(kServer, LT1Mod::kScriptServer);
    Check(lint.find("Mod\\Script.lua:2: WARNING: mod.spawn came in mod.api 4, above the mod.api 3 this script asks for") != std::string::npos &&
          lint.find("Mod\\Script.lua:3: WARNING: OnClientMessage came in mod.api 5, above the mod.api 3") != std::string::npos,
          "the static pass flags an API call and an event above the version the script asks for, by line: " + lint);
    Check(lint.find("Mod\\Script.lua:4: WARNING: mod.hud.text is Mod\\ClientScript.lua's; this file has no mod.hud.text") != std::string::npos &&
          lint.find("Mod\\Script.lua:5: WARNING: mod.fnid is not in the API (a misspelling?)") != std::string::npos &&
          lint.find("Mod\\Script.lua:10: WARNING: mod.fnid2 is not in the API") != std::string::npos,
          "it names the other side's call and a misspelt one, and counts lines across a long comment and a long string");
    Check(lint.find("bogus") == std::string::npos, "a mod.x in a comment or a string, short or long, is no call");
    Check(lint.find("Mod\\Script.lua: needs mod.api 5 (OnClientMessage, line 3)\n") != std::string::npos,
          "and it says the version the script needs, with what needs it");
    const std::string lintC = LT1Mod::Script_Lint("function OnDraw() mod.hud.rect(0, 0, 1, 1, 0, 0, 0) end\n"
                                                  "function OnCommand() mod.find('x') mod.server('n') end\n",
                                                  LT1Mod::kScriptClient);
    Check(lintC.find("Mod\\ClientScript.lua:2: WARNING: mod.find is Mod\\Script.lua's") != std::string::npos &&
          lintC.find("needs mod.api 5 (mod.server, line 2); mod.require_api(5) would have an older game say so") != std::string::npos &&
          lintC.find("hud.rect") == std::string::npos,
          "on the client: a server call named as the server's, and a script that asks for no version told which to ask for: " + lintC);

    // The tables the pass reads, against the live API and events: every name
    // a script can reach is one the pass knows, on each side.
    for (int side = 0; side < 2; ++side)
    {
        const bool bClient = side == 1;
        RecHost hs; RecClientHost hc; std::string err;
        const char* kList = "local t = {} for k in pairs(mod) do t[#t + 1] = 'mod.' .. k end\n"
                            "if type(mod.hud) == 'table' then for k in pairs(mod.hud) do t[#t + 1] = 'mod.hud.' .. k end end\n"
                            "print(table.concat(t, ' '))";
        const bool bOk = bClient ? LoadClientScript(kList, hc, err) : LoadScript(kList, hs, err);
        const std::vector<std::string>& got = bClient ? hc.lines : hs.lines;
        const std::string names = bOk && !got.empty() ? got.back().substr(got.back().find(' ') + 1) : "";
        // require is a global, not mod's: named here so the pass sees it (API 7)
        const std::string text = names + "\nrequire('m')\n";
        std::string defs;
        for (const char* e : bClient ? std::vector<const char*>{ "OnClientStart", "OnWorldStart", "OnDraw", "OnCommand", "OnUpdate", "OnServerMessage" }
                                     : std::vector<const char*>{ "OnWorldStart", "OnCreate", "OnDamage", "OnDeath", "OnCommand", "OnWorldEnd",
                                                                 "OnRemove", "OnTrigger", "OnPlayerSpawn", "OnPlayerDeath", "OnPlayerJoin",
                                                                 "OnPlayerLeave", "OnUpdate", "OnRestore", "OnClientMessage" })
            defs += std::string("function ") + e + "() end\n";
        const std::string l = LT1Mod::Script_Lint(text + defs, bClient ? LT1Mod::kScriptClient : LT1Mod::kScriptServer);
        Check(!names.empty() && l.find("WARNING") == std::string::npos &&
              // Version 7's require is both sides', so both newest are the game's.
              l.find("needs mod.api " + std::to_string(LT1Mod::kScriptApi)) != std::string::npos,
              std::string(bClient ? "client" : "server") + ": every live mod call and every event is in the pass's tables, "
              "the newest at the game's mod.api: " + l + names);
        LT1Mod::Script_Clear();
    }

    // The trace, from the manifest.
    MemSource src;
    src.Put("Mod\\Mod.txt", "[Mod]\nId = \"S\"\nGame = \"Shogo\"\n[Options]\nScriptTrace = TRUE\n");
    src.Put("Mod\\Script.lua",
            "function OnCreate(o) mod.find('x') o:health() end\n"
            "function OnCommand(n)\n"
            "  mod.hud(5, 'x')\n"
            "end\n");
    LT1Mod::State st;
    LT1Mod::Load(src, "Shogo", st);
    LT1Mod::SetActive(st);
    RecHost h;
    std::string err;
    Check(st.manifest.scriptTrace && LT1Mod::Script_Load(src, &h, err), "[Options] ScriptTrace = TRUE is read from Mod.txt" + err);
    int obj = 0;
    LT1Mod::ScriptObject o = { &obj, "trooper1", "A", "A" };
    LT1Mod::Script_Create(o);
    Check(h.Has("Mod\\Script.lua: trace: OnCreate(A 'trooper1', false)") && h.Has("Mod\\Script.lua: trace: mod.find(\"x\")") &&
          h.Has("Mod\\Script.lua: trace: obj:health(A 'trooper1')"),
          "with it, each event and each API call is said with its arguments, object methods too: " + Joined(h.lines));
    LT1Mod::Script_Command("go", NULL, 0);
    Check(h.Has("Mod\\Script.lua:3: bad argument #1 to 'mod.hud'"),
          "an API error under the trace still names the script's line and the call: " + Joined(h.lines));
    // A value whose __tostring raises: the trace shows it without calling it, and print's raise
    // is an ordinary script error (TraceArgs and ModPrint hold no C++ string across a raise)
    LT1Mod::Script_Clear();
    MemSource src2;
    src2.Put("Mod\\Mod.txt", "[Mod]\nId = \"S\"\nGame = \"Shogo\"\n[Options]\nScriptTrace = TRUE\n");
    src2.Put("Mod\\Script.lua",
             "bad = setmetatable({}, { __tostring = function() error('tostring raised') end })\n"
             "function OnCommand(n) if n == 'trace' then mod.find(bad) else print('x', bad) end end\n");
    LT1Mod::State st2;
    LT1Mod::Load(src2, "Shogo", st2);
    LT1Mod::SetActive(st2);
    RecHost hb;
    LT1Mod::Script_Load(src2, &hb, err);
    LT1Mod::Script_Command("trace", NULL, 0);
    const bool bTraced = hb.Has("trace: mod.find(table: ") && !hb.Has("tostring raised");
    LT1Mod::Script_WorldStart("W");
    LT1Mod::Script_Command("print", NULL, 0);
    Check(bTraced && hb.Has("Mod\\Script.lua:1: tostring raised") && !hb.Has("Script: x"),
          "a value whose __tostring raises is traced without calling it, and print's raise is a script error like any: " +
          Joined(hb.lines));
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
    RecHost hn;
    LoadScript("function OnCreate(o) mod.find('x') end\nfunction OnCommand() mod.hud(5, 'x') end", hn, err);
    LT1Mod::Script_Create(o);
    LT1Mod::Script_Command("go", NULL, 0);
    Check(!hn.Has("trace:") && hn.Has("Mod\\Script.lua:2: bad argument #1 to 'hud'"), "without it, no trace line, and the same error: " + Joined(hn.lines));
    LT1Mod::Script_Clear();

    // A slow event, by the wall clock, said once a world.
    RecHost hw;
    LoadScript("function OnCommand(n) if n == 'slow' then local x = 0 for i = 1, 3000000 do x = x + i end end end", hw, err);
    LT1Mod::Script_Command("quick", NULL, 0);
    const bool bQuietFast = !hw.Has("took");
    LT1Mod::Script_Command("slow", NULL, 0);
    const bool bSaid = hw.Has("Mod\\Script.lua: OnCommand took") && hw.Has("ms, more than 5");
    hw.lines.clear();
    LT1Mod::Script_Command("slow", NULL, 0);
    const bool bOnce = !hw.Has("took");
    LT1Mod::Script_WorldStart("W");
    LT1Mod::Script_Command("slow", NULL, 0);
    Check(bQuietFast && bSaid && bOnce && hw.Has("OnCommand took"),
          "an event over 5 ms is said with its time, once a world, and a quick one is not");
    LT1Mod::Script_Clear();
    LT1Mod::SetActive(LT1Mod::State());
}

int Selftest()
{
    TestManifest();
    TestHash();
    TestAttributes();
    TestRegistry();
    TestSavePath();
    TestTextLines();
    TestClassDefs();
    TestScript();
    TestScriptV2();
    TestScriptSaves();
    TestScriptSpawn();
    TestScriptV6();
    TestScriptModules();
    TestClientScript();
    TestScriptMessages();
    TestScriptDiagnostics();
    printf("\npassed %d, failed %d\n", g_nPass, g_nFail);
    return g_nFail ? 1 : 0;
}

int Sig(const char* root, const char* game)
{
    DiskSource src(root);
    LT1Mod::State s;
    LT1Mod::Load(src, game, s);
    printf("%s\n", LT1Mod::Signature(s).c_str());
    printf("%s\n", LT1Mod::Describe(s).c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "--selftest") == 0)
        return Selftest();
    if (argc >= 3 && strcmp(argv[1], "--sig") == 0)
        return Sig(argv[2], argc >= 4 ? argv[3] : "Shogo");
    fprintf(stderr, "usage: testLT1Mod --selftest | --sig <root> [game]\n");
    return 2;
}
