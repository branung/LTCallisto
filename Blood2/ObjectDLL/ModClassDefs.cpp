// ModClassDefs.cpp: see ModClassDefs.h

#include "server_de.h"
#include "ModClassDefs.h"
#include "BloodModAI.h"
#include "BloodModWeapons.h"
#include "BloodModExplosions.h"
#include "BloodModProjectiles.h"
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
	bool bAI;
	{
		LT1Mod::EngineSource src(pServer, pServer->GetFileList, pServer->FreeFileList);
		BloodModWeapons_Load(src, sErr);
		bAI = BloodModAI_Load(src, sErr);		// on false the shell's load says why
		// Projectiles.txt's new classes, whose names may be new explosions'
		BloodModExplosions_Load(src, sErr);
		BloodModProjectiles_Load(src, sErr);
	}

	int n = 0;
	std::string sAdded, sSkipped;
	if (bAI)
	{
		std::vector<LT1Mod::ClassName> names;
		for (int i = 0; BloodModAI_ArchetypeName(i); ++i)
			names.push_back(LT1Mod::ClassName(BloodModAI_ArchetypeName(i), BloodModAI_ArchetypeClass(i)));
		n = LT1Mod::AppendClasses(names, pNative, nNative, pOut, nRoom, sAdded, sSkipped);
		if (n)
			pServer->CPrint("Mod\\AI.txt: %d class name%s: %s", n, n == 1 ? "" : "s", sAdded.c_str());
		if (!sSkipped.empty())
			pServer->CPrint("Mod\\AI.txt: not class names: %s", sSkipped.c_str());
	}

	// A new projectile class is its root retail class's code under its name.
	std::vector<LT1Mod::ClassName> projectiles;
	for (int i = 0; i < BloodModProjectile_NewCount(); ++i)
	{
		const int c = BMP_COUNT + i;
		projectiles.push_back(LT1Mod::ClassName(BloodModProjectile_Class(c),
												BloodModProjectile_Class(BloodModProjectile_Root(c))));
	}
	sAdded.clear();
	sSkipped.clear();
	const int nP = LT1Mod::AppendClasses(projectiles, pNative, nNative, pOut + n, nRoom - n, sAdded, sSkipped);
	if (nP)
		pServer->CPrint("Mod\\Projectiles.txt: %d class name%s: %s", nP, nP == 1 ? "" : "s", sAdded.c_str());
	if (!sSkipped.empty())
		pServer->CPrint("Mod\\Projectiles.txt: not class names: %s", sSkipped.c_str());
	return n + nP;
}
