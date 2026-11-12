// ModClassDefs.cpp: see ModClassDefs.h

#include "server_de.h"
#include "ModClassDefs.h"
#include "BloodModAI.h"
#include "BloodModWeapons.h"
#include "lt1mod_classdefs.h"
#include "lt1mod_engine.h"

int ModClassDefs_Append(ILTServer* pServer, ClassDef** pNative, int nNative, ClassDef** pOut, int nRoom)
{
	if (!pServer)
		return 0;

	// The mod and the tables AI.txt reads, as the server shell will load them
	// again once the engine has the classes (BloodServerShell.cpp).
	LT1Mod::LoadActive(pServer, pServer->GetFileList, pServer->FreeFileList, "Blood2");
	if (!LT1Mod::Active().present)
		return 0;
	std::string sErr;
	{
		LT1Mod::EngineSource src(pServer, pServer->GetFileList, pServer->FreeFileList);
		BloodModWeapons_Load(src, sErr);
		if (!BloodModAI_Load(src, sErr))
			return 0;		// the shell's load says why
	}

	std::vector<LT1Mod::ClassName> names;
	for (int i = 0; BloodModAI_ArchetypeName(i); ++i)
		names.push_back(LT1Mod::ClassName(BloodModAI_ArchetypeName(i), BloodModAI_ArchetypeClass(i)));

	std::string sAdded, sSkipped;
	const int n = LT1Mod::AppendClasses(names, pNative, nNative, pOut, nRoom, sAdded, sSkipped);
	if (n)
		pServer->CPrint("Mod\\AI.txt: %d class name%s: %s", n, n == 1 ? "" : "s", sAdded.c_str());
	if (!sSkipped.empty())
		pServer->CPrint("Mod\\AI.txt: not class names: %s", sSkipped.c_str());
	return n;
}
