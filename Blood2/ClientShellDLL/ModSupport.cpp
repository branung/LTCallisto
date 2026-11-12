#include <string>

#include "ModSupport.h"
#include "SharedDefs.h"
#include "lt1mod_engine.h"

void B2Mod_Init(CClientDE* pClientDE)
{
	if (!pClientDE)
		return;

	// With no Mod\Mod.txt this finds nothing and changes nothing
	LT1Mod::LoadActive(pClientDE, pClientDE->GetFileList, pClientDE->FreeFileList, "Blood2");
	if (LT1Mod::Active().present)
		pClientDE->CPrint("%s", LT1Mod::Describe(LT1Mod::Active()).c_str());
}

DBOOL B2Mod_OnServerSignature(CClientDE* pClientDE, HMESSAGEREAD hMessage)
{
	if (!pClientDE || !hMessage)
		return DFALSE;

	// Copied at once: ReadFromMessageString hands back one of a few rotating buffers
	std::string theirs = pClientDE->ReadFromMessageString(hMessage);
	std::string mine = LT1Mod::Signature(LT1Mod::Active());

	// Answer whatever the server said, so it can tell its host
	HMESSAGEWRITE hWrite = pClientDE->StartMessage(MSG_MOD_SIGNATURE);
	if (hWrite)
	{
		pClientDE->WriteToMessageString(hWrite, mine.c_str());
		pClientDE->EndMessage(hWrite);
	}

	if (mine == theirs)
		return DFALSE;

	pClientDE->CPrint("Mod mismatch: the server has \"%s\", this game has \"%s\" (%s)",
					  theirs.c_str(), mine.c_str(), LT1Mod::Describe(LT1Mod::Active()).c_str());
	return DTRUE;
}

const char* B2Mod_MismatchText()
{
	return "This server is running different mod files. Both machines need the same mod to play together; the console says which each one has.";
}
