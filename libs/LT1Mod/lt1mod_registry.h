#ifndef __LT1MOD_REGISTRY_H__
#define __LT1MOD_REGISTRY_H__

// Registries: names in data, frozen numbers on the wire. Every id a mod can extend travels between
// client and server as a number, is stored in levels and saves, and indexes tables both modules
// compile. So retail entries keep their retail numbers (AddBase, then Freeze), and a mod's new
// entries are appended after them in file order (Append). Two machines with the same mod files
// number the appended entries the same way.
//
// Table<T> holds one T per id: the compiled retail table is the base, and Apply() merges an
// attribute file onto it. A tag naming an existing entry overrides only the keys it gives; a new
// tag appends.
//
// Header-only and standard-library-only, so a game module links nothing new for it.

#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

#include "lt1mod_attrib.h"

namespace LT1Mod
{

class Registry
{
public:
    // maxIds bounds the whole id space, base included: 256 for an id sent as a byte
    explicit Registry(uint32_t maxIds) : m_maxIds(maxIds), m_numBase(0), m_bFrozen(false) {}

    // A retail entry. Ids are dense in call order, so callers add their enum in order and give an
    // unused slot a name ("UNUSED1") so the numbering can't slip. Returns the id, or -1 if frozen,
    // full, or the name is empty or taken.
    int AddBase(const char* name)
    {
        if (m_bFrozen)
            return -1;
        int id = Add(name);
        if (id >= 0)
            m_numBase = (uint32_t)m_names.size();
        return id;
    }

    // Ends the base set; Append is only legal after it
    void Freeze() { m_bFrozen = true; }
    bool IsFrozen() const { return m_bFrozen; }

    // A mod's new entry. Returns the id, or -1 with the reason in *pErr.
    int Append(const char* name, std::string* pErr = 0)
    {
        if (!m_bFrozen)
        {
            if (pErr) *pErr = "registry not frozen";
            return -1;
        }
        if (!name || !name[0])
        {
            if (pErr) *pErr = "empty name";
            return -1;
        }
        if (Find(name) >= 0)
        {
            if (pErr) *pErr = std::string("name already registered: ") + name;
            return -1;
        }
        if (m_names.size() >= m_maxIds)
        {
            if (pErr) *pErr = std::string("registry full, cannot add ") + name;
            return -1;
        }
        return Add(name);
    }

    // Case-insensitive, as attribute tags are; -1 if absent
    int Find(const char* name) const
    {
        if (!name)
            return -1;
        for (size_t i = 0; i < m_names.size(); ++i)
            if (EqualNoCase(m_names[i].c_str(), name))
                return (int)i;
        return -1;
    }

    const char* Name(uint32_t id) const { return id < m_names.size() ? m_names[id].c_str() : 0; }
    uint32_t    Count() const           { return (uint32_t)m_names.size(); }
    uint32_t    NumBase() const         { return m_numBase; }
    bool        IsBase(uint32_t id) const { return id < m_numBase; }
    uint32_t    MaxIds() const          { return m_maxIds; }

    static bool EqualNoCase(const char* a, const char* b)
    {
        for (;; ++a, ++b)
        {
            unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
            if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
            if (ca != cb) return false;
            if (!ca) return true;
        }
    }

private:
    int Add(const char* name)
    {
        if (!name || !name[0] || Find(name) >= 0 || m_names.size() >= m_maxIds)
            return -1;
        m_names.push_back(name);
        return (int)m_names.size() - 1;
    }

    std::vector<std::string>    m_names;
    uint32_t                    m_maxIds;
    uint32_t                    m_numBase;
    bool                        m_bFrozen;
};

template <class T>
class Table
{
public:
    explicit Table(uint32_t maxIds) : m_reg(maxIds) {}

    int AddBase(const char* name, const T& value)
    {
        int id = m_reg.AddBase(name);
        if (id >= 0)
            m_values.push_back(value);
        return id;
    }

    void Freeze() { m_reg.Freeze(); }

    // Merges `attrs` onto the table in file order. An existing name calls
    // apply(entry, attrs, tag, false); a new name appends a copy of `newDefault` and calls
    // apply(entry, attrs, tag, true). apply reads only the keys present, so naming one field
    // changes one field. Returns false at the first tag that can't be appended, with the reason
    // in *pErr; entries already applied stay.
    template <class ApplyFn>
    bool Apply(const Attributes& attrs, const T& newDefault, ApplyFn apply, std::string* pErr = 0)
    {
        const std::vector<std::string>& tags = attrs.Tags();
        for (size_t i = 0; i < tags.size(); ++i)
        {
            const char* tag = tags[i].c_str();
            int id = m_reg.Find(tag);
            bool bNew = id < 0;
            if (bNew)
            {
                id = m_reg.Append(tag, pErr);
                if (id < 0)
                    return false;
                m_values.push_back(newDefault);
            }
            apply(m_values[(size_t)id], attrs, tag, bNew);
        }
        return true;
    }

    const Registry& Names() const       { return m_reg; }
    uint32_t        Count() const       { return m_reg.Count(); }
    const T&        operator[](uint32_t id) const { return m_values[id]; }
    T&              operator[](uint32_t id)       { return m_values[id]; }

private:
    Registry        m_reg;
    std::vector<T>  m_values;
};

} // namespace LT1Mod

#endif // __LT1MOD_REGISTRY_H__
