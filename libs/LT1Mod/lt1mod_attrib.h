#ifndef __LT1MOD_ATTRIB_H__
#define __LT1MOD_ATTRIB_H__

// An attribute file as ButeMgr reads one, the format of NOLF2's Attributes\*.txt and of Mod\*.txt:
//
//     [Tag]
//     IntKey    = 12
//     FloatKey  = 0.5
//     BoolKey   = TRUE
//     StringKey = "text"
//     VecKey    = <1.0, 2.0, 3.0>
//
// A thin wrapper that keeps ButeMgr's headers out of game code, which compiles against 1998
// headers with their own DWORD, and gives the tags in file order, which is the same on every
// compiler. ButeMgr's rules carry through: lookups are case-insensitive, a repeated [Tag] fails
// the parse, and a repeated key keeps its first value.

#include <string>
#include <vector>

namespace LT1Mod
{

class FileSource;

class Attributes
{
public:
    Attributes();
    ~Attributes();

    // Parses a whole file. On failure Error() says why, with the file name and line, and the
    // object holds nothing.
    bool Parse(const char* data, size_t len, const char* name);

    // Reads `path` from `src` and parses it; a missing file is a failure that says so
    bool Load(FileSource& src, const char* path);

    const std::string& Error() const { return m_error; }

    // Every tag, in the order the file declares them, with the file's case
    const std::vector<std::string>& Tags() const { return m_tags; }

    bool Has(const char* tag, const char* key = 0) const;

    // Typed reads. An absent key gives `def`. A key of the wrong type gives `def` and records the
    // mismatch in Error(), so a typo is reported instead of read as zero. Ints read as floats.
    int         GetInt(const char* tag, const char* key, int def) const;
    float       GetFloat(const char* tag, const char* key, float def) const;
    // The same number unrounded, for a value the game keeps as a double
    double      GetDouble(const char* tag, const char* key, double def) const;
    bool        GetBool(const char* tag, const char* key, bool def) const;
    std::string GetString(const char* tag, const char* key, const char* def) const;
    bool        GetVector(const char* tag, const char* key, float out[3]) const;

    // The keys of one tag, sorted case-insensitively (ButeMgr doesn't keep their order)
    std::vector<std::string> Keys(const char* tag) const;

private:
    Attributes(const Attributes&);
    Attributes& operator=(const Attributes&);

    struct Impl;
    Impl*                       m_pImpl;
    std::vector<std::string>    m_tags;
    mutable std::string         m_error;
};

} // namespace LT1Mod

#endif // __LT1MOD_ATTRIB_H__
