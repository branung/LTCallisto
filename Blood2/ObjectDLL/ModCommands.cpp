// ----------------------------------------------------------------------- //
//
// MODULE  : ModCommands.cpp
//
// PURPOSE : See ModCommands.h
//
// ----------------------------------------------------------------------- //

#include "ModCommands.h"
#include "Trigger.h"			// SendTriggerMsgToObjects
#include "ObjectUtilities.h"	// GetRandom, the module's own rand() stream
#include "lt1mod.h"
#include "lt1mod_engine.h"
#include "lt1mod_command.h"
#include "lt1mod_startcmds.h"
#include "BloodModWeapons.h"

#include <stdlib.h>
#include <string>
#include <vector>

namespace
{

// The engine and the game behind NOLF1's calls
class BloodCommandHost : public LT1Mod::CommandHost
{
public:
	int Parse(const char* pCommand, const char** pNewCommandPos, char* argBuffer, const char** argPointers, int* nArgs)
	{
		return g_pServerDE ? g_pServerDE->Parse(pCommand, pNewCommandPos, argBuffer, argPointers, nArgs) : 0;
	}
	void SendMessage(const char* pName, const char* pMsg)
	{
		if (!g_pServerDE) return;
		HSTRING hName = g_pServerDE->CreateString((char*)pName);
		HSTRING hMsg  = g_pServerDE->CreateString((char*)pMsg);
		SendTriggerMsgToObjects(DNULL, hName, hMsg);
		g_pServerDE->FreeString(hName);
		g_pServerDE->FreeString(hMsg);
	}
	int RandomInt(int lo, int hi) { return GetRandom(lo, hi); }
	// Blood II has no float GetRandom; NOLF1's, from the same rand() stream
	float RandomFloat(float lo, float hi) { return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX); }
	void Print(const char* pLine) { if (g_pServerDE) g_pServerDE->CPrint("%s", pLine); }
};

BloodCommandHost g_Host;
LT1Mod::CommandMgr g_Mgr(g_Host);

void PrintLines(const std::string& sPrefix, const std::string& sText)
{
	size_t nStart = 0;
	while (g_pServerDE && nStart < sText.size())
	{
		size_t nEnd = sText.find('\n', nStart);
		if (nEnd == std::string::npos) nEnd = sText.size();
		g_pServerDE->CPrint("%s%s", sPrefix.c_str(), sText.substr(nStart, nEnd - nStart).c_str());
		nStart = nEnd + 1;
	}
}

} // namespace

static void PrintServerLine(const char* pLine)
{
	if (g_pServerDE) g_pServerDE->CPrint("%s", pLine);
}

void ModWeapons_LoadFile()
{
	if (!g_pServerDE) return;
	LT1Mod::EngineSource src(g_pServerDE, g_pServerDE->GetFileList, g_pServerDE->FreeFileList);
	BloodModWeapons_LoadAndReport(src, PrintServerLine, "Server: ");
}

void ModCommands_LoadFile()
{
	if (!g_pServerDE) return;
	LT1Mod::EngineSource src(g_pServerDE, g_pServerDE->GetFileList, g_pServerDE->FreeFileList);
	std::string sErr;
	if (!LT1Mod::StartCommands_Load(src, sErr))
		g_pServerDE->CPrint("Server: %s", sErr.c_str());
	else
		PrintLines("Server: Mod\\Commands.txt: ", LT1Mod::StartCommands_Summary());
}

void ModCommands_Process(const char* pCmd)
{
	g_Mgr.SetTrace(LT1Mod::StartCommands_Trace());
	g_Mgr.Process(pCmd);
}

void ModCommands_Update(DFLOAT fFrameTime)
{
	if (g_Mgr.NumPending()) g_Mgr.Update((float)fFrameTime);
}

void ModCommands_WorldStarted(const char* pWorld, DBOOL bRestoring)
{
	if (bRestoring) return;		// the save brings its own pending commands
	g_Mgr.Clear();
	const std::vector<std::string> start = LT1Mod::StartCommands_For(pWorld);
	for (size_t i = 0; i < start.size(); ++i)
		if (!start[i].empty()) ModCommands_Process(start[i].c_str());
}

void ModCommands_Save(HMESSAGEWRITE hWrite)
{
	if (!LT1Mod::StartCommands_Loaded() || !g_pServerDE || !hWrite) return;
	const std::string blob = g_Mgr.Save();
	g_pServerDE->WriteToMessageDWord(hWrite, (DDWORD)blob.size());
	for (size_t i = 0; i < blob.size(); ++i)
		g_pServerDE->WriteToMessageByte(hWrite, (DBYTE)blob[i]);
}

void ModCommands_Load(HMESSAGEREAD hRead, DBOOL bApply)
{
	if (!LT1Mod::StartCommands_Loaded() || !g_pServerDE || !hRead) return;
	const DDWORD n = g_pServerDE->ReadFromMessageDWord(hRead);
	std::string blob;
	blob.reserve(n);
	for (DDWORD i = 0; i < n; ++i)
		blob += (char)g_pServerDE->ReadFromMessageByte(hRead);
	if (bApply) g_Mgr.Load(blob);
}

// ----------------------------------------------------------------------- //
//
//	Command
//
// ----------------------------------------------------------------------- //

BEGIN_CLASS(Command)
	ADD_STRINGPROP(Command, "")
	ADD_STRINGPROP(StartCommand, "")
END_CLASS_DEFAULT(Command, B2BaseClass, NULL, NULL)

Command::Command() : B2BaseClass(OT_NORMAL)
{
	m_hstrCommand		= DNULL;
	m_hstrStartCommand	= DNULL;
	m_bStarted			= DFALSE;
}

Command::~Command()
{
	if (!g_pServerDE) return;
	if (m_hstrCommand) g_pServerDE->FreeString(m_hstrCommand);
	if (m_hstrStartCommand) g_pServerDE->FreeString(m_hstrStartCommand);
}

DDWORD Command::EngineMessageFn(DDWORD messageID, void *pData, DFLOAT fData)
{
	CServerDE* pServerDE = g_pServerDE;

	switch(messageID)
	{
		case MID_PRECREATE:
		{
			DDWORD dwRet = B2BaseClass::EngineMessageFn(messageID, pData, fData);
			if (pServerDE && (fData == PRECREATE_WORLDFILE || fData == PRECREATE_STRINGPROP))
			{
				GenericProp genProp;
				if (pServerDE->GetPropGeneric("Command", &genProp) == DE_OK && genProp.m_String[0])
					m_hstrCommand = pServerDE->CreateString(genProp.m_String);
				if (pServerDE->GetPropGeneric("StartCommand", &genProp) == DE_OK && genProp.m_String[0])
					m_hstrStartCommand = pServerDE->CreateString(genProp.m_String);
			}
			return dwRet;
		}

		case MID_INITIALUPDATE:
		{
			if (pServerDE && fData != INITIALUPDATE_SAVEGAME && m_hstrStartCommand && !m_bStarted)
				pServerDE->SetNextUpdate(m_hObject, 0.001f);
			break;
		}

		case MID_UPDATE:
		{
			if (pServerDE && m_hstrStartCommand && !m_bStarted)
			{
				m_bStarted = DTRUE;
				ModCommands_Process(pServerDE->GetStringData(m_hstrStartCommand));
			}
			if (pServerDE) pServerDE->SetNextUpdate(m_hObject, 0.0f);
			break;
		}

		case MID_SAVEOBJECT:
		{
			HMESSAGEWRITE hWrite = (HMESSAGEWRITE)pData;
			if (pServerDE && hWrite)
			{
				pServerDE->WriteToMessageHString(hWrite, m_hstrCommand);
				pServerDE->WriteToMessageHString(hWrite, m_hstrStartCommand);
				pServerDE->WriteToMessageByte(hWrite, m_bStarted);
			}
			break;
		}

		case MID_LOADOBJECT:
		{
			HMESSAGEREAD hRead = (HMESSAGEREAD)pData;
			if (pServerDE && hRead)
			{
				if (m_hstrCommand) pServerDE->FreeString(m_hstrCommand);
				if (m_hstrStartCommand) pServerDE->FreeString(m_hstrStartCommand);
				m_hstrCommand		= pServerDE->ReadFromMessageHString(hRead);
				m_hstrStartCommand	= pServerDE->ReadFromMessageHString(hRead);
				m_bStarted			= (DBOOL)pServerDE->ReadFromMessageByte(hRead);
			}
			break;
		}

		default : break;
	}

	return B2BaseClass::EngineMessageFn(messageID, pData, fData);
}

DDWORD Command::ObjectMessageFn(HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead)
{
	if (g_pServerDE && messageID == MID_TRIGGER && m_hstrCommand)
		ModCommands_Process(g_pServerDE->GetStringData(m_hstrCommand));
	return B2BaseClass::ObjectMessageFn(hSender, messageID, hRead);
}
