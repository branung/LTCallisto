#include "lt1mod_classdefs.h"
#include "ltserverobj.h"

#include <deque>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif

namespace LT1Mod
{

namespace
{

// Stable addresses: the engine keeps the ClassDef pointers and their names
std::deque<ClassDef>	s_Defs;
std::deque<std::string>	s_Names;

bool SameNoCase(const char* a, const char* b)
{
#ifdef _WIN32
	return _stricmp(a, b) == 0;
#else
	return strcasecmp(a, b) == 0;
#endif
}

} // namespace

int AppendClasses(const std::vector<ClassName>& classes, ClassDef** pNative, int nNative,
                  ClassDef** pOut, int nRoom, std::string& sAdded, std::string& sSkipped)
{
	int nOut = 0;
	for (size_t i = 0; i < classes.size(); ++i)
	{
		const std::string& name = classes[i].first;
		const std::string& host = classes[i].second;
		ClassDef* pHost = NULL;
		bool bTaken = false;
		for (int c = 0; c < nNative; ++c)
		{
			if (SameNoCase(pNative[c]->m_ClassName, host.c_str())) pHost = pNative[c];
			if (SameNoCase(pNative[c]->m_ClassName, name.c_str())) bTaken = true;
		}
		for (int c = 0; c < nOut; ++c)
			if (SameNoCase(pOut[c]->m_ClassName, name.c_str())) bTaken = true;
		const char* pWhy = bTaken ? "a class has the name" : !pHost ? "no such host class" : nOut >= nRoom ? "no room" : NULL;
		if (pWhy)
		{
			sSkipped += (sSkipped.empty() ? "" : ", ") + name + " (" + pWhy + ")";
			continue;
		}

		s_Names.push_back(name);
		s_Defs.push_back(*pHost);
		ClassDef& d			= s_Defs.back();
		d.m_ClassName		= s_Names.back().c_str();
		d.m_ParentClass		= pHost;
		d.m_ConstructFn		= NULL;		// the parent's (SetupClassNullFunctions)
		d.m_DestructFn		= NULL;
		d.m_PluginFn		= NULL;
		d.m_nProps			= 0;		// the parent's (cb_FindVarFull)
		d.m_Props			= NULL;
		d.m_pInternal[0]	= NULL;
		d.m_pInternal[1]	= NULL;
		pOut[nOut++] = &d;
		sAdded += (sAdded.empty() ? "" : ", ") + name + " (" + host + ")";
	}
	return nOut;
}

} // namespace LT1Mod
