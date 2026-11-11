#include "lt1mod_strings.h"
#include "lt1mod.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace LT1Mod
{

namespace
{

const size_t kMaxText = 4000;		// under the engine's 5000-byte LoadString buffer
const int kMaxId = 65535;			// a Win32 string resource id is 16 bits

bool EqualNoCase(const char* a, const char* b)
{
	for (; *a && *b; ++a, ++b)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
	return *a == *b;
}

} // namespace

StringTable::StringTable(const StringName* pNames, int nNames, const char* pGame, const char* pRef)
	: m_pNames(pNames), m_nNames(nNames), m_pGame(pGame), m_pRef(pRef)
{
}

const char* StringTable::Get(int nId) const
{
	std::map<int, std::string>::const_iterator it = m_Strings.find(nId);
	return it == m_Strings.end() ? 0 : it->second.c_str();
}

const char* StringTable::Name(int nId) const
{
	for (int i = 0; i < m_nNames; ++i)
		if (m_pNames[i].nId == nId) return m_pNames[i].pName;
	return 0;
}

int StringTable::Id(const char* pName) const
{
	for (int i = 0; pName && i < m_nNames; ++i)
		if (EqualNoCase(m_pNames[i].pName, pName)) return m_pNames[i].nId;
	return -1;
}

std::string StringTable::Quote(const char* p)
{
	std::string out = "\"";
	for (; p && *p; ++p)
	{
		switch (*p)
		{
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			default:   out += *p; break;
		}
	}
	return out + "\"";
}

int StringTable::ProbeId() const
{
	for (size_t i = 0; i < m_Order.size(); ++i)
		if (Get(m_Order[i]) && !strchr(Get(m_Order[i]), '%')) return m_Order[i];
	return -1;
}

void StringTable::Clear()
{
	m_Strings.clear();
	m_Order.clear();
}

std::string StringTable::Summary() const
{
	if (!Active()) return std::string();
	char b[64];
	snprintf(b, sizeof(b), "%u string%s:", (unsigned)m_Order.size(), m_Order.size() == 1 ? "" : "s");
	std::string out = b;
	for (size_t i = 0; i < m_Order.size() && i < 5; ++i)
	{
		const char* pName = Name(m_Order[i]);
		snprintf(b, sizeof(b), "%d", m_Order[i]);
		out += std::string(i ? ", " : " ") + (pName ? pName : b);
	}
	if (m_Order.size() > 5) out += ", ...";
	return out;
}

bool StringTable::Load(FileSource& src, std::string& sError)
{
	Clear();
	sError.clear();
	if (!LT1Mod::IsActive())
		return true;

	const char* const kPath = "Mod\\Strings.txt";
	std::string text;
	if (!src.ReadFile(kPath, text))
		return true;

	std::map<int, std::string> strings;
	std::vector<int> order;
	char buf[512];
	size_t pos = 0;
	int nLine = 0;
	while (pos < text.size())
	{
		++nLine;
		size_t eol = text.find('\n', pos);
		if (eol == std::string::npos) eol = text.size();
		const std::string line = text.substr(pos, eol - pos);
		pos = eol + 1;

		// The key
		size_t i = 0;
		while (i < line.size() && isspace((unsigned char)line[i])) ++i;
		if (i == line.size() || line.compare(i, 2, "//") == 0) continue;
		const size_t k0 = i;
		while (i < line.size() && (isalnum((unsigned char)line[i]) || line[i] == '_')) ++i;
		const std::string key = line.substr(k0, i - k0);
		while (i < line.size() && isspace((unsigned char)line[i])) ++i;
		if (key.empty() || i == line.size() || line[i] != '=')
		{
			snprintf(buf, sizeof(buf), "%s: line %d is not `IDS_NAME = \"text\"` or `<id> = \"text\"`", kPath, nLine);
			sError = buf;
			return false;
		}
		++i;

		int nId;
		if (isdigit((unsigned char)key[0]))
		{
			if (key.find_first_not_of("0123456789") != std::string::npos || key.size() > 5 || atoi(key.c_str()) > kMaxId)
			{
				snprintf(buf, sizeof(buf), "%s: line %d: %s is not a string id (0 to %d)", kPath, nLine, key.c_str(), kMaxId);
				sError = buf;
				return false;
			}
			nId = atoi(key.c_str());
		}
		else
		{
			nId = Id(key.c_str());
			if (nId < 0)
			{
				snprintf(buf, sizeof(buf), "%s: line %d: %s is not one of %s's IDS_ names (Shared/ClientRes.h; "
						 "%s lists every string)", kPath, nLine, key.c_str(), m_pGame, m_pRef);
				sError = buf;
				return false;
			}
		}

		// The text
		while (i < line.size() && isspace((unsigned char)line[i])) ++i;
		if (i == line.size() || line[i] != '"')
		{
			snprintf(buf, sizeof(buf), "%s: line %d: the text after = must be in double quotes", kPath, nLine);
			sError = buf;
			return false;
		}
		++i;
		std::string value;
		bool bClosed = false;
		for (; i < line.size(); ++i)
		{
			const char c = line[i];
			if (c == '"') { bClosed = true; ++i; break; }
			if (c != '\\') { if (c != '\r') value += c; continue; }
			if (++i == line.size()) break;
			switch (line[i])
			{
				case 'n':  value += '\n'; break;
				case 'r':  value += '\r'; break;
				case 't':  value += '\t'; break;
				case '"':  value += '"'; break;
				case '\\': value += '\\'; break;
				default:
					snprintf(buf, sizeof(buf), "%s: line %d: \\%c is not an escape (\\n \\r \\t \\\" \\\\)", kPath, nLine, line[i]);
					sError = buf;
					return false;
			}
		}
		if (!bClosed)
		{
			snprintf(buf, sizeof(buf), "%s: line %d: the text has no closing quote", kPath, nLine);
			sError = buf;
			return false;
		}
		while (i < line.size() && isspace((unsigned char)line[i])) ++i;
		if (i < line.size() && line.compare(i, 2, "//") != 0)
		{
			snprintf(buf, sizeof(buf), "%s: line %d: something follows the closing quote", kPath, nLine);
			sError = buf;
			return false;
		}
		if (value.size() > kMaxText)
		{
			snprintf(buf, sizeof(buf), "%s: line %d: the text is longer than %u characters", kPath, nLine, (unsigned)kMaxText);
			sError = buf;
			return false;
		}
		if (strings.count(nId))
		{
			const char* pName = Name(nId);
			snprintf(buf, sizeof(buf), "%s: line %d: string %d%s%s is given twice", kPath, nLine, nId,
					 pName ? " " : "", pName ? pName : "");
			sError = buf;
			return false;
		}
		strings[nId] = value;
		order.push_back(nId);
	}

	m_Strings.swap(strings);
	m_Order.swap(order);
	return true;
}

} // namespace LT1Mod
