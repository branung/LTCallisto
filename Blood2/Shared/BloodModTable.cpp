#include "server_de.h"
#include "BloodModTable.h"

#include "lt1mod.h"
#include "lt1mod_attrib.h"
#include "lt1mod_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace BloodMod
{

namespace
{

// The vector kind's layout: WeaponDefs.h's VectorInit, three floats
struct Vec3 { float x, y, z; };

template <class T> T& At(char* p, const Field& f) { return *(T*)(p + f.nOffset); }
template <class T> const T& At(const char* p, const Field& f) { return *(const T*)(p + f.nOffset); }

const char* NameOf(const Named* p, int n)
{
	for (; p->pName; ++p)
		if (p->nValue == n) return p->pName;
	return 0;
}

bool EqualNoCase(const char* a, const char* b) { return LT1Mod::Registry::EqualNoCase(a, b); }

size_t SizeOf(Kind e)
{
	switch (e)
	{
		case K_STRING: case K_STRNULL:	return sizeof(char*);
		case K_NAMED:					return sizeof(DBYTE);
		case K_WORD:					return sizeof(D_WORD);
		case K_DWORD:					return sizeof(DDWORD);
		case K_INT:						return sizeof(int);
		case K_FLOAT:					return sizeof(DFLOAT);
		case K_BOOL:					return sizeof(DBOOL);
		case K_VECTOR:					return sizeof(Vec3);
	}
	return 0;
}

} // namespace

std::string DF(float f)
{
	char b[96];
	std::string s;
	bool bDone = false;
	for (int p = 1; p <= 9 && !bDone; ++p)
	{
		snprintf(b, sizeof(b), "%.*g", p, f);
		bDone = !strchr(b, 'e') && strtof(b, 0) == f;
	}
	for (int p = 1; p <= 60 && !bDone; ++p)
	{
		snprintf(b, sizeof(b), "%.*f", p, f);
		bDone = strtof(b, 0) == f;
	}
	s = b;
	if (s.find('.') == std::string::npos) s += ".0";
	return s;
}

Table::Table(const char* pPath, const char* pWhat, const char* pHint, const char* pRef,
			 const Array* pArrays, int nArrays, int nCount, int nFirstId,
			 const Entry* pEntries, int nEntries, const Field* pFields, int nFields,
			 int nCapacity, const char* pBaseKey, const char* pNewWord)
	: m_pPath(pPath), m_pWhat(pWhat), m_pHint(pHint), m_pRef(pRef),
	  m_Arrays(pArrays, pArrays + nArrays), m_nCount(nCount), m_nFirstId(nFirstId),
	  m_pEntries(pEntries), m_nEntries(nEntries), m_pFields(pFields), m_nFields(nFields),
	  m_nCapacity(nCapacity > nCount ? nCapacity : nCount), m_pBaseKey(pBaseKey), m_pNewWord(pNewWord)
{
}

void Table::TakeRetail()
{
	if (!m_Retail.empty()) return;
	m_Retail.resize(m_Arrays.size());
	for (size_t a = 0; a < m_Arrays.size(); ++a)
	{
		const char* p = (const char*)m_Arrays[a].pBase;
		m_Retail[a].assign(p, p + m_Arrays[a].nStride * (size_t)m_nCount);
	}
}

char* Table::Element(int nArray, int nId, bool bRetail)
{
	TakeRetail();
	const size_t nOff = m_Arrays[(size_t)nArray].nStride * (size_t)(nId - m_nFirstId);
	return (bRetail ? &m_Retail[(size_t)nArray][0] : (char*)m_Arrays[(size_t)nArray].pBase) + nOff;
}

bool Table::Valid(std::string& sWhy) const
{
	sWhy.clear();
	char buf[256];
	for (int i = 0; i < m_nFields; ++i)
	{
		const Field& f = m_pFields[i];
		if (f.nArray < 0 || f.nArray >= (int)m_Arrays.size() ||
			f.nOffset + SizeOf(f.eKind) > m_Arrays[(size_t)f.nArray].nStride)
		{
			snprintf(buf, sizeof(buf), "%s: the key %s is outside its array's element (BloodMod table definition)",
					 m_pPath, f.pKey);
			sWhy = buf;
			return false;
		}
	}
	return true;
}

const char* Table::Name(int nId) const
{
	for (int i = 0; i < m_nEntries; ++i)
		if (m_pEntries[i].nId == nId) return m_pEntries[i].pName;
	const int n = nId - (m_nFirstId + m_nCount);
	return (n >= 0 && n < (int)m_NewNames.size()) ? m_NewNames[(size_t)n].c_str() : 0;
}

int Table::Find(const char* pName) const
{
	if (pName)
	{
		for (int i = 0; i < m_nEntries; ++i)
			if (EqualNoCase(m_pEntries[i].pName, pName)) return m_pEntries[i].nId;
		for (size_t n = 0; n < m_NewNames.size(); ++n)
			if (EqualNoCase(m_NewNames[n].c_str(), pName)) return m_nFirstId + m_nCount + (int)n;
	}
	return m_nFirstId - 1;
}

int Table::Base(int nId) const
{
	const int n = nId - (m_nFirstId + m_nCount);
	return (n >= 0 && n < (int)m_NewBases.size()) ? m_NewBases[(size_t)n] : m_nFirstId - 1;
}

int Table::Retail(int nId) const
{
	// Each new entry's Base comes before it, so the walk ends.
	while (Base(nId) >= m_nFirstId) nId = Base(nId);
	return nId;
}

const char* Table::Key(int nField) const
{
	return (nField >= 0 && nField < m_nFields) ? m_pFields[nField].pKey : 0;
}

std::string Table::ValueAt(const char* p, const Field& f) const
{
	char b[64];
	switch (f.eKind)
	{
		case K_STRING:
		case K_STRNULL:
		{
			const char* s = At<char*>(p, f);
			return std::string("\"") + (s ? s : "") + "\"";
		}
		case K_NAMED:
		{
			const int n = At<DBYTE>(p, f);
			const char* s = NameOf(f.pNames, n);
			if (s) return std::string("\"") + s + "\"";
			snprintf(b, sizeof(b), "%d", n);		// none retail has (tests/Blood2Tables)
			return b;
		}
		case K_WORD:	snprintf(b, sizeof(b), "%u", (unsigned)At<D_WORD>(p, f)); return b;
		case K_DWORD:	snprintf(b, sizeof(b), "%u", (unsigned)At<DDWORD>(p, f)); return b;
		case K_INT:		snprintf(b, sizeof(b), "%d", At<int>(p, f)); return b;
		case K_FLOAT:	return DF(At<DFLOAT>(p, f));
		case K_BOOL:	return At<DBOOL>(p, f) ? "TRUE" : "FALSE";
		case K_VECTOR:
		{
			const Vec3& v = At<Vec3>(p, f);
			return "<" + DF(v.x) + ", " + DF(v.y) + ", " + DF(v.z) + ">";
		}
	}
	return std::string();
}

std::string Table::Value(int nId, int nField, bool bRetail)
{
	std::string sWhy;
	if (nId < m_nFirstId || nId >= m_nFirstId + (bRetail ? m_nCount : Total()) || nField < 0 || nField >= m_nFields || !Valid(sWhy))
		return std::string();
	const Field& f = m_pFields[nField];
	return ValueAt(Element(f.nArray, nId, bRetail), f);
}

void Table::Clear()
{
	TakeRetail();
	for (size_t a = 0; a < m_Arrays.size(); ++a)
	{
		memcpy(m_Arrays[a].pBase, &m_Retail[a][0], m_Retail[a].size());
		// The new entries' room, back to the zeros the module started with.
		const size_t nAll = m_Arrays[a].nStride * (size_t)m_nCapacity;
		memset((char*)m_Arrays[a].pBase + m_Retail[a].size(), 0, nAll - m_Retail[a].size());
	}
	m_NewNames.clear();
	m_NewBases.clear();
}

bool Table::Load(LT1Mod::FileSource& src, std::string& sError)
{
	Clear();
	sError.clear();
	if (!Valid(sError))
		return false;
	if (!LT1Mod::IsActive())
		return true;

	std::string text;
	if (!src.ReadFile(m_pPath, text))
		return true;							// a mod need not touch this table

	LT1Mod::Attributes a;
	if (!a.Parse(text.data(), text.size(), m_pPath))
	{
		sError = a.Error();
		return false;
	}

	// Built into scratch copies of the arrays, with its strings, and committed only if the whole
	// file is good. The copies are the arrays' whole capacity: retail's entries, then zeros where
	// new ones go.
	std::vector<std::vector<char> > table(m_Arrays.size());
	for (size_t i = 0; i < m_Arrays.size(); ++i)
	{
		table[i] = m_Retail[i];
		table[i].resize(m_Arrays[i].nStride * (size_t)m_nCapacity, 0);
	}
	std::deque<std::string> strings;
	struct Pending { int nId; int nField; size_t nString; };	// a string to point at, once committed
	std::vector<Pending> pending;
	char buf[512];

	// One key of one section into the scratch table; false, with sError, on a bad one
	auto ApplyKey = [&](const char* tag, const char* key, int nId) -> bool
	{
		int nField = -1;
		for (int i = 0; i < m_nFields && nField < 0; ++i)
			if (EqualNoCase(m_pFields[i].pKey, key)) nField = i;
		if (nField < 0)
		{
			snprintf(buf, sizeof(buf), "%s: [%s] has no field \"%s\" (%s lists them all)", m_pPath, tag, key, m_pRef);
			sError = buf;
			return false;
		}
		const Field& f = m_pFields[nField];
		// A string this entry sets replaces any it was waiting for: its Base's, copied with the
		// Base's values below
		if (f.eKind == K_STRING || f.eKind == K_STRNULL)
			for (size_t p = pending.size(); p-- > 0;)
				if (pending[p].nId == nId && pending[p].nField == nField) pending.erase(pending.begin() + (long)p);
		char* w = &table[(size_t)f.nArray][0] + m_Arrays[(size_t)f.nArray].nStride * (size_t)(nId - m_nFirstId);

		const std::string before = a.Error();
		bool bRange = true;
		switch (f.eKind)
		{
			case K_STRING:
			case K_STRNULL:
			{
				const std::string s = a.GetString(tag, key, "");
				bRange = s.size() <= kMaxString;
				if (f.eKind == K_STRNULL && s.empty())
					At<char*>(w, f) = 0;
				else
				{
					strings.push_back(s);
					Pending p = { nId, nField, strings.size() - 1 };
					pending.push_back(p);
				}
				break;
			}
			case K_NAMED:
			{
				const std::string s = a.GetString(tag, key, "");
				int n = -1;
				for (const Named* p = f.pNames; p->pName && n < 0; ++p)
					if (EqualNoCase(s.c_str(), p->pName)) n = p->nValue;
				bRange = n >= 0;
				if (bRange) At<DBYTE>(w, f) = (DBYTE)n;
				break;
			}
			case K_WORD:
			{
				const int n = a.GetInt(tag, key, 0);
				bRange = n >= 0 && n <= 65535;
				At<D_WORD>(w, f) = (D_WORD)n;
				break;
			}
			case K_DWORD:
			{
				const int n = a.GetInt(tag, key, 0);
				bRange = n >= 0;
				At<DDWORD>(w, f) = (DDWORD)n;
				break;
			}
			case K_INT:
			{
				const int n = a.GetInt(tag, key, 0);
				bRange = n >= 0;
				At<int>(w, f) = n;
				break;
			}
			case K_FLOAT:
			{
				const float x = a.GetFloat(tag, key, 0.0f);
				bRange = x >= 0.0f && x <= 1e30f;		// and not NaN
				At<DFLOAT>(w, f) = x;
				break;
			}
			case K_BOOL:
				At<DBOOL>(w, f) = a.GetBool(tag, key, false) ? DTRUE : DFALSE;
				break;
			case K_VECTOR:
			{
				float v[3] = { 0, 0, 0 };
				if (!a.GetVector(tag, key, v) && a.Error() == before)
					bRange = false;
				Vec3& o = At<Vec3>(w, f);
				o.x = v[0]; o.y = v[1]; o.z = v[2];
				break;
			}
		}
		if (a.Error() != before)
		{
			sError = std::string(m_pPath) + ": " + a.Error();
			return false;
		}
		if (!bRange)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:%s is out of range", m_pPath, tag, key);
			if (f.eKind == K_STRING || f.eKind == K_STRNULL)
				snprintf(buf, sizeof(buf), "%s: [%s]:%s is longer than %u characters", m_pPath, tag, key,
						 (unsigned)kMaxString);
			else if (f.eKind == K_NAMED)
			{
				std::string list;
				for (const Named* p = f.pNames; p->pName; ++p)
					list += std::string(p == f.pNames ? "" : ", ") + "\"" + p->pName + "\"";
				snprintf(buf, sizeof(buf), "%s: [%s]:%s must be one of %s", m_pPath, tag, key, list.c_str());
			}
			sError = buf;
			return false;
		}
		return true;
	};

	// The sections: retail entries first, in file order, then the new ones in file order, so a
	// new entry starts from its Base's values as this mod leaves them, retail's overrides included
	struct New { std::string sName; int nBase; };
	std::vector<New> news;
	std::vector<std::string> retailTags, newTags;
	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const int nId = Find(tag);
		if (nId >= m_nFirstId && nId < m_nFirstId + m_nCount)
		{
			if (m_pBaseKey && a.Has(tag, m_pBaseKey))
			{
				snprintf(buf, sizeof(buf), "%s: [%s] is retail's; %s is only for a new one", m_pPath, tag, m_pBaseKey);
				sError = buf;
				return false;
			}
			retailTags.push_back(tags[t]);
			continue;
		}
		if (!m_pBaseKey || !a.Has(tag, m_pBaseKey))
		{
			if (m_pBaseKey)
				snprintf(buf, sizeof(buf), "%s: [%s] is not %s (%s; %s lists them), and has no %s to make it a new one",
						 m_pPath, tag, m_pWhat, m_pHint, m_pRef, m_pBaseKey);
			else
				snprintf(buf, sizeof(buf), "%s: [%s] is not %s (%s; %s lists them)", m_pPath, tag, m_pWhat, m_pHint, m_pRef);
			sError = buf;
			return false;
		}
		// A new one's name is a word a console, a level and an AI.txt can all write: letters,
		// digits and _, at most 31
		bool bWord = tags[t].size() <= 31 && !tags[t].empty();
		for (size_t c = 0; c < tags[t].size() && bWord; ++c)
			bWord = isalnum((unsigned char)tags[t][c]) || tags[t][c] == '_';
		if (!bWord)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]: a new name is letters, digits and _, at most 31", m_pPath, tag);
			sError = buf;
			return false;
		}
		const std::string before = a.Error();
		const std::string sBase = a.GetString(tag, m_pBaseKey, "");
		if (a.Error() != before)
		{
			sError = std::string(m_pPath) + ": " + a.Error();
			return false;
		}
		int nBase = Find(sBase.c_str());
		if (nBase < m_nFirstId || nBase >= m_nFirstId + m_nCount)
		{
			nBase = m_nFirstId - 1;
			for (size_t n = 0; n < news.size(); ++n)
				if (EqualNoCase(news[n].sName.c_str(), sBase.c_str())) nBase = m_nFirstId + m_nCount + (int)n;
		}
		if (nBase < m_nFirstId)
		{
			snprintf(buf, sizeof(buf), "%s: [%s]:%s \"%s\" is not %s, nor a new one above it", m_pPath, tag, m_pBaseKey,
					 sBase.c_str(), m_pWhat);
			sError = buf;
			return false;
		}
		if (m_nCount + (int)news.size() >= m_nCapacity)
		{
			snprintf(buf, sizeof(buf), "%s: [%s] is one new entry too many: there is room for %d", m_pPath, tag,
					 m_nCapacity - m_nCount);
			sError = buf;
			return false;
		}
		for (size_t n = 0; n < news.size(); ++n)
			if (EqualNoCase(news[n].sName.c_str(), tag))
			{
				snprintf(buf, sizeof(buf), "%s: [%s] is given twice", m_pPath, tag);
				sError = buf;
				return false;
			}
		New n = { tags[t], nBase };
		news.push_back(n);
		newTags.push_back(tags[t]);
	}

	for (size_t t = 0; t < retailTags.size(); ++t)
	{
		const char* tag = retailTags[t].c_str();
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
			if (!ApplyKey(tag, keys[k].c_str(), Find(tag))) return false;
	}
	for (size_t n = 0; n < news.size(); ++n)
	{
		const int nId = m_nFirstId + m_nCount + (int)n;
		for (size_t i = 0; i < m_Arrays.size(); ++i)
		{
			const size_t nStride = m_Arrays[i].nStride;
			memcpy(&table[i][0] + nStride * (size_t)(nId - m_nFirstId),
				   &table[i][0] + nStride * (size_t)(news[n].nBase - m_nFirstId), nStride);
		}
		// Strings the Base is still waiting for are the new one's too.
		const size_t nWaiting = pending.size();
		for (size_t p = 0; p < nWaiting; ++p)
			if (pending[p].nId == news[n].nBase)
			{
				Pending q = pending[p];
				q.nId = nId;
				pending.push_back(q);
			}
		const char* tag = newTags[n].c_str();
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
			if (!EqualNoCase(keys[k].c_str(), m_pBaseKey) && !ApplyKey(tag, keys[k].c_str(), nId)) return false;
	}

	// Commit: the strings move to the module's store first, so the pointers written into the
	// table are the ones that stay. Each array is copied by its scratch copy's size, measured from
	// the stride and the capacity, never by a product of the games' unparenthesized macros, which
	// once copied 17 weapons and two bytes of the 18th.
	const size_t nFirst = m_Strings.size();
	for (size_t i = 0; i < strings.size(); ++i)
		m_Strings.push_back(strings[i]);
	for (size_t i = 0; i < pending.size(); ++i)
	{
		const Field& f = m_pFields[pending[i].nField];
		char* w = &table[(size_t)f.nArray][0] + m_Arrays[(size_t)f.nArray].nStride * (size_t)(pending[i].nId - m_nFirstId);
		At<char*>(w, f) = (char*)m_Strings[nFirst + pending[i].nString].c_str();
	}
	for (size_t i = 0; i < m_Arrays.size(); ++i)
		memcpy(m_Arrays[i].pBase, &table[i][0], table[i].size());
	for (size_t n = 0; n < news.size(); ++n)
	{
		m_NewNames.push_back(news[n].sName);
		m_NewBases.push_back(news[n].nBase);
	}
	return true;
}

std::string Table::Summary()
{
	std::string out;
	for (int nId = m_nFirstId; nId < m_nFirstId + m_nCount; ++nId)
	{
		std::string line;
		for (int f = 0; f < m_nFields; ++f)
		{
			const std::string v = Value(nId, f);
			if (v == Value(nId, f, true)) continue;
			line += std::string(line.empty() ? " " : ", ") + m_pFields[f].pKey + " " + v;
		}
		if (!line.empty())
			out += (out.empty() ? "" : "\n") + std::string(Name(nId)) + ":" + line;
	}
	// A new entry: its number, its Base, and what it sets over its Base.
	for (int nId = m_nFirstId + m_nCount; nId < m_nFirstId + Total(); ++nId)
	{
		char b[64];
		snprintf(b, sizeof(b), ": new %s %d, %s \"", m_pNewWord ? m_pNewWord : "entry", nId, m_pBaseKey);
		std::string line = std::string(Name(nId)) + b + Name(Base(nId)) + "\"";
		for (int f = 0; f < m_nFields; ++f)
		{
			const std::string v = Value(nId, f);
			if (v != Value(Base(nId), f))
				line += std::string(", ") + m_pFields[f].pKey + " " + v;
		}
		out += (out.empty() ? "" : "\n") + line;
	}
	return out;
}

std::string Table::Reference()
{
	std::string out, sWhy;
	if (!Valid(sWhy))
		return out;
	for (int nId = m_nFirstId; nId < m_nFirstId + m_nCount; ++nId)
	{
		out += std::string("\n[") + Name(nId) + "]\n";
		for (int f = 0; f < m_nFields; ++f)
			out += std::string(m_pFields[f].pKey) + " = " + Value(nId, f, true) + "\n";
	}
	return out;
}

void Table::LoadAndReport(LT1Mod::FileSource& src, void (*pPrint)(const char* pLine), const char* pPrefix)
{
	std::string sErr;
	const bool bOk = Load(src, sErr);
	if (!pPrint) return;
	PrintWrapped(bOk ? Summary() : sErr,
				 std::string(pPrefix ? pPrefix : "") + (bOk ? std::string(m_pPath) + ": " : std::string()), pPrint);
}

void PrintWrapped(const std::string& sText, const std::string& sHead, void (*pPrint)(const char* pLine))
{
	// A line at a time, and each kept under the engine's 511 character console line: one entry's
	// summary can be longer
	const size_t kMaxPrint = 400;
	size_t nStart = 0;
	while (pPrint && nStart < sText.size())
	{
		size_t nEnd = sText.find('\n', nStart);
		if (nEnd == std::string::npos) nEnd = sText.size();
		std::string sRest = sText.substr(nStart, nEnd - nStart);
		bool bFirst = true;
		while (!sRest.empty())
		{
			size_t nCut = sRest.size();
			if (nCut > kMaxPrint)
			{
				const size_t nComma = sRest.rfind(", ", kMaxPrint);
				nCut = (nComma != std::string::npos && nComma > 0) ? nComma + 1 : kMaxPrint;
			}
			pPrint((sHead + (bFirst ? "" : "    ") + sRest.substr(0, nCut)).c_str());
			sRest = sRest.substr(nCut);
			if (!sRest.empty() && sRest[0] == ' ') sRest.erase(0, 1);
			bFirst = false;
		}
		nStart = nEnd + 1;
	}
}

} // namespace BloodMod
