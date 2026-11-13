// One of Blood II's compiled tables, overridable by a mod.
//
// Blood II's tables are arrays of structs its consumers read directly (g_WeaponDefaults,
// gCharacterValues), so a mod's values are written into the array itself, once, when the module
// loads the mod, from a copy of retail's taken first: every load starts from retail and an error
// leaves retail. A Table holds the rules the tables share; each table's own file lists its
// sections (the entries, by name) and its keys (a field of one of its arrays, by offset and kind).
//
// A table may be split over several arrays indexed alike: entry i is element i of each.
//
// The file is an attribute file (lt1mod_attrib.h). A section names an entry, without case; a key a
// field. A bad file (an unknown section or key, a value of the wrong type or out of range) loads
// nothing, and the error says where. A string a mod gives is kept for the life of the module and
// never freed: the games keep the tables' char* pointers, not copies.

#ifndef __BLOODMODTABLE_H__
#define __BLOODMODTABLE_H__

#include <stddef.h>
#include <deque>
#include <string>
#include <vector>

namespace LT1Mod { class FileSource; }

namespace BloodMod
{

enum Kind
{
	K_STRING,		// char*, never NULL in retail: "" is ""
	K_STRNULL,		// char*, NULL in retail where an entry has none, never "": "" is NULL
	K_NAMED,		// an unsigned char, one of pNames, written as a quoted name
	K_WORD,			// an unsigned short, 0-65535
	K_DWORD,		// an unsigned int, 0 or more
	K_INT,			// int, 0 or more
	K_FLOAT,		// float, 0 or more
	K_BOOL,			// unsigned int, TRUE or FALSE (DBOOL)
	K_VECTOR,		// three floats
};

struct Named { const char* pName; int nValue; };	// a K_NAMED field's names; { 0, 0 } ends them

struct Field
{
	const char*		pKey;
	int				nArray;		// which of the table's arrays
	size_t			nOffset;	// in that array's element
	Kind			eKind;
	const Named*	pNames;
};

struct Entry { int nId; const char* pName; };		// a section: its id and its name

struct Array { void* pBase; size_t nStride; };		// element i at pBase + i * nStride

class Table
{
public:
	// pPath is the file ("Mod\\Weapons.txt"). An error names an entry as pWhat ("a Blood 2
	// weapon"), says what the sections are with pHint ("the WEAP_ names without the prefix") and
	// points at pRef, the reference file. Ids run from nFirstId; entry id is element
	// (id - nFirstId) of each array.
	//
	// With pBaseKey ("Base"), a section naming no entry adds one: it starts as a copy of the entry
	// its pBaseKey names (retail's, or a new one above it in the file) as this mod leaves that
	// entry, then takes its own keys. New entries are numbered after retail's in file order, while
	// the arrays have room: nCapacity elements each, which the arrays must hold. pNewWord names
	// one in the summary ("weapon": "new weapon 52").
	Table(const char* pPath, const char* pWhat, const char* pHint, const char* pRef,
		  const Array* pArrays, int nArrays, int nCount, int nFirstId,
		  const Entry* pEntries, int nEntries, const Field* pFields, int nFields,
		  int nCapacity = 0, const char* pBaseKey = 0, const char* pNewWord = 0);

	// The section name of entry nId, or NULL; an id by name, without case, or nFirstId - 1
	const char*	Name(int nId) const;
	int			Find(const char* pName) const;
	int			FirstId() const	{ return m_nFirstId; }
	int			Count() const	{ return m_nCount; }		// retail's
	int			Total() const	{ return m_nCount + (int)m_NewNames.size(); }	// with the mod's new ones
	int			Capacity() const	{ return m_nCapacity; }
	// A new entry's Base, or nFirstId - 1 for any other id; and the retail entry at the end of
	// its chain of Bases, or the id itself for any other
	int			Base(int nId) const;
	int			Retail(int nId) const;

	// The keys, in table order; NULL past the last
	const char*	Key(int nField) const;
	// Entry nId's value for one key, as the file writes it: the current table's (a new entry's
	// too), or retail's
	std::string	Value(int nId, int nField, bool bRetail = false);

	// Every field fits inside its array's element; sWhy names one that does not. A table that
	// fails this is a mistake in its definition, so it loads nothing and answers nothing.
	bool		Valid(std::string& sWhy) const;

	// Loads pPath through src, after setting the table back to retail. An absent file, or no
	// active mod, is no overrides and no error.
	bool		Load(LT1Mod::FileSource& src, std::string& sError);
	void		Clear();
	// One line per entry that differs from retail, "NAME: Key value, ...", then one per new entry,
	// "NAME: new weapon 52, Base \"SHOTGUN\", Key value"
	std::string	Summary();
	// Every entry's every retail value, as the file: the reference
	std::string	Reference();
	std::string	Current();		// Reference()'s text for the table as it now is, mod and all
	// Load, then print the summary or the error a console line at a time
	void		LoadAndReport(LT1Mod::FileSource& src, void (*pPrint)(const char* pLine), const char* pPrefix);

	// The longest string a mod may give: LT1's MAX_CS_FILENAME_LEN less its NUL, the smaller of
	// the two engines' filename buffers
	static const size_t kMaxString = 99;

private:
	void		TakeRetail();
	char*		Element(int nArray, int nId, bool bRetail);
	std::string	ValueAt(const char* pElem, const Field& f) const;

	const char*					m_pPath;
	const char*					m_pWhat;
	const char*					m_pHint;
	const char*					m_pRef;
	std::vector<Array>			m_Arrays;
	int							m_nCount, m_nFirstId;
	const Entry*				m_pEntries;
	int							m_nEntries;
	const Field*				m_pFields;
	int							m_nFields;
	int							m_nCapacity;
	const char*					m_pBaseKey;
	const char*					m_pNewWord;
	std::vector<std::string>	m_NewNames;		// the mod's new entries, by id after retail's
	std::vector<int>			m_NewBases;
	std::vector<std::vector<char> >	m_Retail;	// one copy per array, taken before the first write
	std::deque<std::string>		m_Strings;		// every string a mod gave, never freed
};

// A float in the fewest digits that read back as the same float, with no exponent (ButeMgr reads
// none) and always a decimal point
std::string DF(float f);

// sText a line at a time through pPrint, each after sHead, and each cut at a comma to stay under
// the engine's console line
void PrintWrapped(const std::string& sText, const std::string& sHead, void (*pPrint)(const char* pLine));

} // namespace BloodMod

#endif // __BLOODMODTABLE_H__
