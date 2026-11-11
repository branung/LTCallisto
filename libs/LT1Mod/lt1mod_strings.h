// A game's text strings, overridable by a mod's Mod\Strings.txt.
//
// One `key = "text"` per line, `//` comments, \n \r \t \" \\ escapes. A key is one of the game's
// IDS_ names (any case) or a decimal id up to 65535; a text is at most 4000 characters, under
// the engine's 5000 byte LoadString buffer. Not ButeMgr, whose string token cannot hold the
// games' quotes, newlines and long text. A bad line loads nothing, and the error names the line.
//
// The client's ILTClient::FormatString asks the shell's LT1Mod_ClientString export first, and
// the shell answers from its StringTable. The engine formats the text as it formats a resource
// string, so a mod's string must keep the original's %1!s! inserts.

#ifndef __LT1MOD_STRINGS_H__
#define __LT1MOD_STRINGS_H__

#include <map>
#include <string>
#include <vector>

namespace LT1Mod
{

class FileSource;

struct StringName { const char* pName; int nId; };

class StringTable
{
public:
	// pGame names the game in an error ("Shogo"); pRef is the reference file an error points at
	StringTable(const StringName* pNames, int nNames, const char* pGame, const char* pRef);

	// The mod's text for an id, or NULL: none loaded, or not one it sets
	const char*	Get(int nId) const;
	bool		Active() const	{ return !m_Strings.empty(); }

	// The IDS_ name of an id (the first, if two share it), or NULL; an id by name, any case, or -1
	const char*	Name(int nId) const;
	int			Id(const char* pName) const;
	int			NameCount() const	{ return m_nNames; }
	const char*	NameAt(int i) const	{ return (i >= 0 && i < m_nNames) ? m_pNames[i].pName : 0; }

	// The text as Strings.txt writes it: quoted, escaped
	static std::string Quote(const char* pText);

	// One id the loaded mod overrides, for a shell's check that the engine answers with it: the
	// first whose text has no % insert; -1 when none
	int			ProbeId() const;

	// Loads Mod\Strings.txt through src, after clearing. An absent file, or no active mod, is no
	// overrides and no error.
	bool		Load(FileSource& src, std::string& sError);
	void		Clear();
	// "3 strings: IDS_A, IDS_B, IDS_C", the first five named; "" when none
	std::string	Summary() const;

private:
	const StringName*		m_pNames;
	int						m_nNames;
	const char*				m_pGame;
	const char*				m_pRef;
	std::map<int, std::string>	m_Strings;
	std::vector<int>		m_Order;			// ids as the file gave them, for the summary
};

} // namespace LT1Mod

#endif // __LT1MOD_STRINGS_H__
