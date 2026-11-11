// Attributes, on ButeMgr. The only file in the library that includes ButeMgr, and it does so the
// way ButeMgr's own stdafx.h does under _NOMFC, so ButeMgr's types never meet a game's headers.

#if defined(WIN32)
#include <windows.h>
#endif
#include "mfcstub.h"
#include "butemgr.h"

#include <algorithm>
#include <ctype.h>

#include "lt1mod.h"
#include "lt1mod_attrib.h"

namespace LT1Mod
{

namespace
{

// ButeMgr keeps its last message in m_sErrorString either way; a no-op display keeps it from
// reaching any default that might open a window
void QuietDisplay(const char*) {}

bool LessNoCase(const std::string& a, const std::string& b)
{
    // Not std::min: windows.h defines a min macro
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i)
    {
        int ca = tolower((unsigned char)a[i]), cb = tolower((unsigned char)b[i]);
        if (ca != cb)
            return ca < cb;
    }
    return a.size() < b.size();
}

bool EqualNoCase(const char* a, const char* b)
{
    for (;; ++a, ++b)
    {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return false;
        if (!ca) return true;
    }
}

bool CollectKey(const char* pszKeyName, CButeMgr::CSymTabItem*, void* pContext)
{
    ((std::vector<std::string>*)pContext)->push_back(pszKeyName);
    return true;
}

} // namespace

struct Attributes::Impl
{
    CButeMgr bm;
};

Attributes::Attributes() : m_pImpl(new Impl)
{
    m_pImpl->bm.Init(QuietDisplay);
}

Attributes::~Attributes()
{
    m_pImpl->bm.Term();
    delete m_pImpl;
}

// ButeMgr scans each token, comments included, into a 4096-byte buffer with no bound check, and a
// longer line crashed it. Lines and quoted strings that could overflow are refused before parsing.
static const size_t kMaxLine = 4090;
static const size_t kMaxToken = 4000;

static bool TooLong(const char* data, size_t len, size_t& lineOut)
{
    size_t lineLen = 0, quoted = 0, line = 1;
    bool bQuoted = false;
    for (size_t i = 0; i < len; ++i)
    {
        const char c = data[i];
        if (c == '"') { bQuoted = !bQuoted; quoted = 0; }
        else if (bQuoted) ++quoted;
        if (c == '\n') { ++line; lineLen = 0; }
        else ++lineLen;
        if (lineLen > kMaxLine || quoted > kMaxToken)
        {
            lineOut = line;
            return true;
        }
    }
    return false;
}

bool Attributes::Parse(const char* data, size_t len, const char* name)
{
    m_tags.clear();
    m_error.clear();
    m_pImpl->bm.Term();
    m_pImpl->bm.Init(QuietDisplay);

    size_t nLine = 0;
    if (TooLong(data, len, nLine))
    {
        char b[64];
        snprintf(b, sizeof(b), "%u", (unsigned)nLine);
        m_error = std::string(name ? name : "attribute file") + ": line " + b +
                  " is longer than 4090 characters or holds a string longer than 4000, which the attribute parser cannot hold";
        return false;
    }

    // The buffer overload takes a non-const pointer but only copies from it
    if (!m_pImpl->bm.Parse((void*)data, (unsigned long)len, 0, CString(name ? name : "")))
    {
        m_error = m_pImpl->bm.GetErrorString().GetBuffer(0);
        if (m_error.empty())
            m_error = std::string(name ? name : "attribute file") + ": parse failed";
        m_pImpl->bm.Term();
        m_pImpl->bm.Init(QuietDisplay);
        return false;
    }

    int n = m_pImpl->bm.GetNumTagsInFileOrder();
    for (int i = 0; i < n; ++i)
        m_tags.push_back(m_pImpl->bm.GetTagInFileOrder(i));
    return true;
}

bool Attributes::Load(FileSource& src, const char* path)
{
    std::string data;
    if (!src.ReadFile(path, data))
    {
        m_tags.clear();
        m_error = std::string(path) + ": not found";
        return false;
    }
    return Parse(data.data(), data.size(), path);
}

bool Attributes::Has(const char* tag, const char* key) const
{
    if (!tag)
        return false;
    if (!key)
    {
        for (size_t i = 0; i < m_tags.size(); ++i)
            if (EqualNoCase(m_tags[i].c_str(), tag))
                return true;
        return false;
    }
    return m_pImpl->bm.GetType(tag, key) != CButeMgr::NullType;
}

// Each typed read checks the stored type first, so a mismatch is reported here and the default
// comes back
#define LT1MOD_TYPECHECK(cond, what)                                             \
    CButeMgr::SymTypes t = m_pImpl->bm.GetType(tag, key);                        \
    if (t == CButeMgr::NullType)                                                 \
        return def;                                                              \
    if (!(cond))                                                                 \
    {                                                                            \
        m_error = std::string("[") + tag + "]:" + key + " is not " what;         \
        return def;                                                              \
    }

int Attributes::GetInt(const char* tag, const char* key, int def) const
{
    LT1MOD_TYPECHECK(t == CButeMgr::IntType, "an integer")
    return m_pImpl->bm.GetInt(tag, key, def);
}

float Attributes::GetFloat(const char* tag, const char* key, float def) const
{
    // ButeMgr stores a written 0.5 as a double, and only an explicit (float) cast gives
    // FloatType, so all three count as numbers here
    LT1MOD_TYPECHECK(t == CButeMgr::FloatType || t == CButeMgr::IntType ||
                     t == CButeMgr::DoubleType, "a number")
    if (t == CButeMgr::DoubleType)
        return (float)m_pImpl->bm.GetDouble(tag, key, def);
    return m_pImpl->bm.GetFloat(tag, key, def);
}

double Attributes::GetDouble(const char* tag, const char* key, double def) const
{
    LT1MOD_TYPECHECK(t == CButeMgr::FloatType || t == CButeMgr::IntType ||
                     t == CButeMgr::DoubleType, "a number")
    if (t == CButeMgr::DoubleType)
        return m_pImpl->bm.GetDouble(tag, key, def);
    if (t == CButeMgr::IntType)
        return (double)m_pImpl->bm.GetInt(tag, key, (int)def);
    return (double)m_pImpl->bm.GetFloat(tag, key, (float)def);
}

bool Attributes::GetBool(const char* tag, const char* key, bool def) const
{
    LT1MOD_TYPECHECK(t == CButeMgr::BoolType, "TRUE or FALSE")
    return m_pImpl->bm.GetBool(tag, key, def);
}

std::string Attributes::GetString(const char* tag, const char* key, const char* def) const
{
    CButeMgr::SymTypes t = m_pImpl->bm.GetType(tag, key);
    if (t == CButeMgr::NullType)
        return def ? def : "";
    if (t != CButeMgr::StringType)
    {
        m_error = std::string("[") + tag + "]:" + key + " is not a quoted string";
        return def ? def : "";
    }
    return m_pImpl->bm.GetString(tag, key, def ? def : "");
}

bool Attributes::GetVector(const char* tag, const char* key, float out[3]) const
{
    CButeMgr::SymTypes t = m_pImpl->bm.GetType(tag, key);
    if (t == CButeMgr::NullType)
        return false;
    if (t != CButeMgr::VectorType)
    {
        m_error = std::string("[") + tag + "]:" + key + " is not a <x, y, z> vector";
        return false;
    }
    CAVector v = m_pImpl->bm.GetVector(tag, key, CAVector(0, 0, 0));
    out[0] = (float)v.Geti();
    out[1] = (float)v.Getj();
    out[2] = (float)v.Getk();
    return true;
}

#undef LT1MOD_TYPECHECK

std::vector<std::string> Attributes::Keys(const char* tag) const
{
    std::vector<std::string> keys;
    if (tag)
        m_pImpl->bm.GetKeys(tag, CollectKey, &keys);
    std::sort(keys.begin(), keys.end(), LessNoCase);
    return keys;
}

} // namespace LT1Mod
