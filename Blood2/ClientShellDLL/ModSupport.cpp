#include <string>
#include <string.h>

#include "ModSupport.h"
#include "SharedDefs.h"
#include "lt1mod_engine.h"
#include "lt1mod_clientscript.h"
#include "lt1mod_script.h"
#include "BloodModWeapons.h"
#include "BloodModCharacters.h"
#include "BloodModAmmo.h"
#include "BloodModLasers.h"
#include "BloodModRules.h"
#include "BloodModExplosions.h"
#include "BloodModObjectFX.h"		// Mod\ObjectFX.txt
#include "MenuCharacter.h"
#include "BloodModStrings.h"
#include "BloodModTable.h"

static CClientDE* s_pPrintClient = 0;
static void PrintClientLine(const char* pLine)
{
	if (s_pPrintClient) s_pPrintClient->CPrint("%s", pLine);
}

// The client script's mod.server: MSG_MOD_SCRIPT, the name and the payload.
static bool SendScriptMessage(const char* pName, const char* pPayload)
{
	if (!s_pPrintClient) return false;
	HMESSAGEWRITE hWrite = s_pPrintClient->StartMessage(MSG_MOD_SCRIPT);
	if (!hWrite) return false;
	s_pPrintClient->WriteToMessageString(hWrite, const_cast<char*>(pName));
	s_pPrintClient->WriteToMessageString(hWrite, const_cast<char*>(pPayload));
	return s_pPrintClient->EndMessage(hWrite) == DE_OK;
}

void B2Mod_OnScriptMessage(HMESSAGEREAD hMessage)
{
	if (!hMessage) return;
	// ReadString, not ReadFromMessageString, which would cut a 1 KB payload short
	char szName[LT1Mod::kScriptMessageNameBuf], szPayload[LT1Mod::kScriptMessagePayloadBuf];
	hMessage->ReadString(szName, sizeof(szName));
	hMessage->ReadString(szPayload, sizeof(szPayload));
	LT1Mod::ClientScript_ServerMessage(szName, szPayload);
}

void B2Mod_Init(CClientDE* pClientDE)
{
	if (!pClientDE)
		return;

	// With no Mod\Mod.txt this finds nothing and changes nothing
	LT1Mod::LoadActive(pClientDE, pClientDE->GetFileList, pClientDE->FreeFileList, "Blood2");
	if (LT1Mod::Active().present)
		pClientDE->CPrint("%s", LT1Mod::Describe(LT1Mod::Active()).c_str());

	// The mod's weapon values, into this module's g_WeaponDefaults; the server shell loads the
	// same file into its own
	LT1Mod::EngineSource src(pClientDE, pClientDE->GetFileList, pClientDE->FreeFileList);
	s_pPrintClient = pClientDE;
	BloodModWeapons_LoadAndReport(src, PrintClientLine, "");

	// The mod's characters, which the server loads too, so both number a new one alike; the
	// client wants the ids and the menu's pictures
	BloodModCharacters_LoadAndReport(src, PrintClientLine, "");
	BuildCharacterMenuInfo();

	// The ammo table and the loose rules, which the server loads too: the client reads the casings
	// and muzzle smoke, and the camera, movement and effect constants. Before the effect manager's
	// Init, which sizes its lists from [Effects]'s caps.
	BloodModAmmo_LoadAndReport(src, PrintClientLine, "");
	BloodModLasers_LoadAndReport(src, PrintClientLine, "");
	BloodModRules_LoadAndReport(src, PrintClientLine, "");
	// The mod's explosions, drawn by ExplosionFX.cpp
	BloodModExplosions_LoadAndReport(src, PrintClientLine, "");
	// And its object effects' particle recipes, by ObjectFX.cpp
	BloodModObjectFX_LoadAndReport(src, PrintClientLine, "");

	// The mod's strings, which the engine's FormatString asks this shell for through
	// ModStringsExport.cpp
	std::string sErr;
	if (!BloodModStrings_Load(src, sErr))
		pClientDE->CPrint("%s", sErr.c_str());
	else
		BloodMod::PrintWrapped(BloodModStrings_Summary(), "Mod\\Strings.txt: ", PrintClientLine);

	// Whether the engine asks: it is engine code this shell cannot see, so ask once and say what
	// came back. An engine that does not look the export up answers with CRes's text, and the
	// mod's strings would be ignored.
	const int nProbe = BloodModString_ProbeId();
	if (nProbe >= 0)
	{
		HSTRING hProbe = pClientDE->FormatString(nProbe);
		const char* pGot = hProbe ? pClientDE->GetStringData(hProbe) : DNULL;
		const char* pName = BloodModString_Name(nProbe);
		if (pGot && strcmp(pGot, BloodModString(nProbe)) == 0)
			pClientDE->CPrint("Mod\\Strings.txt: the engine answers %s with the mod's text", pName ? pName : "a string");
		else
			pClientDE->CPrint("Mod\\Strings.txt: WARNING: the engine answers %s with %s: this engine does not "
							  "read a mod's strings", pName ? pName : "a string", pGot ? "the original text" : "nothing");
		if (hProbe) pClientDE->FreeString(hProbe);
	}

	// Its client script, if it has one: text in the typeface Blood 2's MessageMgr draws in
	LT1Mod::ClientScript_Load(pClientDE, "Arial", SendScriptMessage);
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
