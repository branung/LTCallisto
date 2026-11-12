#include "ModScript.h"
#include "ModCommands.h"
#include "BaseCharacter.h"
#include "PlayerObj.h"			// a player's HCLIENT
#include "SharedDefs.h"			// MSG_MOD_SCRIPT, SMSG_CONSOLEMESSAGE
#include "server_de.h"
#include "ObjectUtilities.h"		// DamageObject, PlaySoundFromPos, GetRandom
#include "SoundTypes.h"			// SOUNDPRIORITY_MISC_MEDIUM
#include "lt1mod_engine.h"
#include "lt1mod_script.h"
#include "BloodModAI.h"			// mod.spawn: the archetypes
#include <string.h>

#include <string>

namespace
{

class BloodScriptHost : public LT1Mod::ScriptHost
{
public:
	virtual void Print(const char* pLine)
	{
		if (g_pServerDE) g_pServerDE->CPrint("%s", pLine);
	}
	virtual void Command(const char* pCmd)
	{
		ModCommands_Process(pCmd);
	}
	virtual double Time()
	{
		return g_pServerDE ? g_pServerDE->GetTime() : 0.0;
	}
	// An object is the CBaseCharacter the event was raised for, valid while it
	// runs (lt1mod_script.h).
	virtual bool GetHealth(void* hObj, float& fHealth)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p) return false;
		fHealth = (float)p->GetDestructable()->GetHitPoints();
		return true;
	}
	virtual bool SetHealth(void* hObj, float fHealth)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p || p->IsDead()) return false;
		p->GetDestructable()->SetHitPoints(fHealth);
		return true;
	}
	virtual bool GetPos(void* hObj, float& x, float& y, float& z)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p || !g_pServerDE || !p->m_hObject) return false;
		DVector v;
		g_pServerDE->GetObjectPos(p->m_hObject, &v);
		x = v.x; y = v.y; z = v.z;
		return true;
	}
	// Version 2 (lt1mod_script.h).
	virtual bool GetMaxHealth(void* hObj, float& f)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p) return false;
		f = p->GetDestructable()->GetMaxHitPoints();
		return true;
	}
	// The game's damage path: an MID_DAMAGE the character sends itself, of
	// DAMAGE_TYPE_NORMAL, at its own position.  So OnDamage and OnDeath come
	// from the same handler a weapon's do.
	virtual bool Damage(void* hObj, float fAmount)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p || !p->m_hObject || p->IsDead() || !g_pServerDE) return false;
		DVector vDir, vPos;
		VEC_SET(vDir, 0.0f, -1.0f, 0.0f);
		g_pServerDE->GetObjectPos(p->m_hObject, &vPos);
		DamageObject(p->m_hObject, p, p->m_hObject, fAmount, vDir, vPos, DAMAGE_TYPE_NORMAL);
		return true;
	}
	virtual bool SendTrigger(void* hObj, const char* pMsg)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p || !p->m_hObject || !g_pServerDE) return false;
		HSTRING hMsg = g_pServerDE->CreateString((char*)pMsg);
		HMESSAGEWRITE hWrite = g_pServerDE->StartMessageToObject(DNULL, p->m_hObject, MID_TRIGGER);
		g_pServerDE->WriteToMessageHString(hWrite, hMsg);
		g_pServerDE->EndMessage(hWrite);
		g_pServerDE->FreeString(hMsg);
		return true;
	}
	virtual bool Teleport(void* hObj, float x, float y, float z)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p || !p->m_hObject || !g_pServerDE) return false;
		DVector v;
		VEC_SET(v, x, y, z);
		g_pServerDE->TeleportObject(p->m_hObject, &v);
		return true;
	}
	virtual bool Remove(void* hObj)
	{
		CBaseCharacter* p = (CBaseCharacter*)hObj;
		if (!p || !p->m_hObject || !g_pServerDE) return false;
		g_pServerDE->RemoveObject(p->m_hObject);
		return true;
	}
	virtual int RandomInt(int lo, int hi) { return GetRandom(lo, hi); }
	// One player's line: the console line of the server shell's
	// ConsoleMessage (BloodServerShell.cpp), which the client reads into 512
	// bytes.  Every player's: the engine's broadcast print.
	virtual bool Hud(void* hPlayer, const char* pText)
	{
		if (!g_pServerDE) return false;
		if (!hPlayer) { g_pServerDE->BPrint("%s", pText); return true; }
		HCLIENT hClient = static_cast<CPlayerObj*>(static_cast<CBaseCharacter*>(hPlayer))->GetClient();
		if (!hClient) return false;
		std::string line(pText);
		if (line.size() > 511) line.resize(511);
		HMESSAGEWRITE hWrite = g_pServerDE->StartMessage(hClient, SMSG_CONSOLEMESSAGE);
		if (!hWrite) return false;
		g_pServerDE->WriteToMessageString(hWrite, const_cast<char*>(line.c_str()));
		g_pServerDE->EndMessage(hWrite);
		return true;
	}
	// Version 5: MSG_MOD_SCRIPT to that player's client, or (NULL) every client.
	virtual bool SendToClient(void* hPlayer, const char* pName, const char* pPayload)
	{
		if (!g_pServerDE) return false;
		HCLIENT hClient = NULL;
		if (hPlayer)
		{
			hClient = static_cast<CPlayerObj*>(static_cast<CBaseCharacter*>(hPlayer))->GetClient();
			if (!hClient) return false;
		}
		HMESSAGEWRITE hWrite = g_pServerDE->StartMessage(hClient, MSG_MOD_SCRIPT);
		if (!hWrite) return false;
		g_pServerDE->WriteToMessageString(hWrite, const_cast<char*>(pName));
		g_pServerDE->WriteToMessageString(hWrite, const_cast<char*>(pPayload));
		g_pServerDE->EndMessage(hWrite);
		return true;
	}
	virtual bool Sound(const char* pPath, float x, float y, float z)
	{
		DVector v;
		VEC_SET(v, x, y, z);
		PlaySoundFromPos(&v, pPath, 1000.0f, SOUNDPRIORITY_MISC_MEDIUM);
		return true;
	}
	// Version 4: one of the mod's archetypes, created at a point.  By its own
	// class where there is one, else by its host with the Archetype
	// property, as a Spawner's string does.
	virtual void* Spawn(const char* pArchetype, float x, float y, float z, const char* pProps, std::string& sWhy)
	{
		if (!g_pServerDE) { sWhy = "no server"; return NULL; }
		const int nArch = BloodModAI_FindArchetype(pArchetype);
		if (nArch < 0) { sWhy = "no such archetype in Mod\\AI.txt"; return NULL; }
		std::string props = pProps ? pProps : "";
		HCLASS hClass = g_pServerDE->GetClass((char*)pArchetype);
		if (!hClass)
		{
			hClass = g_pServerDE->GetClass((char*)BloodModAI_ArchetypeClass(nArch));
			props = std::string("Archetype ") + pArchetype + (props.empty() ? "" : " ") + props;
		}
		if (!hClass) { sWhy = "no class to make it with"; return NULL; }
		ObjectCreateStruct ocs;
		INIT_OBJECTCREATESTRUCT(ocs);
		VEC_SET(ocs.m_Pos, x, y, z);
		ROT_INIT(ocs.m_Rotation);
		char szProps[512];
		strncpy(szProps, props.c_str(), sizeof(szProps) - 1);
		szProps[sizeof(szProps) - 1] = '\0';
		void* p = g_pServerDE->CreateObjectProps(hClass, &ocs, szProps);
		if (!p) sWhy = "the engine made nothing";
		return p;
	}
};

BloodScriptHost s_Host;

bool Fill(CBaseCharacter* pChar, const char* pArchetype, LT1Mod::ScriptObject& o, char* szClass, size_t nClass)
{
	if (!pChar || !pChar->m_hObject || !pArchetype || !pArchetype[0] || !g_pServerDE || !LT1Mod::Script_Loaded())
		return false;
	szClass[0] = '\0';
	HCLASS hClass = g_pServerDE->GetObjectClass(pChar->m_hObject);
	if (hClass) g_pServerDE->GetClassName(hClass, szClass, (uint32)nClass);
	const char* pName = g_pServerDE->GetObjectName(pChar->m_hObject);
	o.hObj = pChar;
	o.pName = pName ? pName : "";
	o.pArchetype = pArchetype;
	o.pClass = szClass;
	return true;
}

} // namespace

void ModScript_Load()
{
	if (!g_pServerDE) return;
	LT1Mod::EngineSource src(g_pServerDE, g_pServerDE->GetFileList, g_pServerDE->FreeFileList);
	std::string sErr;
	if (!LT1Mod::Script_Load(src, &s_Host, sErr))
	{
		for (size_t b = 0, e; b < sErr.size(); b = e + 1)
		{
			e = sErr.find('\n', b);
			if (e == std::string::npos) e = sErr.size();
			g_pServerDE->CPrint("Server: %s", sErr.substr(b, e - b).c_str());
		}
		return;
	}
	if (!LT1Mod::Script_Loaded()) return;
	const std::string sum = LT1Mod::Script_Summary();
	for (size_t b = 0, e; b < sum.size(); b = e + 1)
	{
		e = sum.find('\n', b);
		if (e == std::string::npos) e = sum.size();
		g_pServerDE->CPrint("Server: Mod\\Script.lua: %s", sum.substr(b, e - b).c_str());
	}
}

void ModScript_WorldStarted(const char* pWorld, DBOOL bRestoring)
{
	std::string world = pWorld ? pWorld : "";
	const size_t slash = world.find_last_of("\\/");
	if (slash != std::string::npos) world = world.substr(slash + 1);
	// A restored save raises OnRestore, with the save's mod.save
	LT1Mod::Script_WorldStart(world.c_str(), bRestoring ? true : false);
}

void ModScript_Adopted(CBaseCharacter* pChar, const char* pArchetype)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass))) LT1Mod::Script_Adopt(o);
}

void ModScript_Save(HMESSAGEWRITE hWrite)
{
	if (!LT1Mod::Script_Present() || !g_pServerDE || !hWrite) return;
	const std::string blob = LT1Mod::Script_SaveBlob();
	g_pServerDE->WriteToMessageDWord(hWrite, (DDWORD)blob.size());
	for (size_t i = 0; i < blob.size(); ++i)
		g_pServerDE->WriteToMessageByte(hWrite, (DBYTE)blob[i]);
}

void ModScript_Load(HMESSAGEREAD hRead, DBOOL bApply)
{
	if (!LT1Mod::Script_Present() || !g_pServerDE || !hRead) return;
	const DDWORD n = g_pServerDE->ReadFromMessageDWord(hRead);
	std::string blob;
	blob.reserve(n);
	for (DDWORD i = 0; i < n; ++i)
		blob += (char)g_pServerDE->ReadFromMessageByte(hRead);
	if (bApply) LT1Mod::Script_RestoreBlob(blob);
}

void ModScript_Created(CBaseCharacter* pChar, const char* pArchetype)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass))) LT1Mod::Script_Create(o);
}

void ModScript_Damaged(CBaseCharacter* pChar, const char* pArchetype, DFLOAT fAmount)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass))) LT1Mod::Script_Damage(o, fAmount);
}

void ModScript_Died(CBaseCharacter* pChar, const char* pArchetype)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass))) LT1Mod::Script_Death(o);
}

void ModScript_Triggered(CBaseCharacter* pChar, const char* pArchetype, const char* pMsg, HOBJECT hSender)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (!Fill(pChar, pArchetype, o, szClass, sizeof(szClass))) return;
	const char* pFrom = (hSender && g_pServerDE) ? g_pServerDE->GetObjectName(hSender) : DNULL;
	LT1Mod::Script_Trigger(o, pMsg, pFrom ? pFrom : "");
}

void ModScript_Removed(void* pObj)
{
	LT1Mod::Script_Removed(pObj);
}

namespace
{

// A player, as the script sees one: no archetype, its own name.
bool FillPlayer(CBaseCharacter* pPlayer, const char* pName, LT1Mod::ScriptObject& o, char* szClass, size_t nClass)
{
	if (!pPlayer || !pPlayer->m_hObject || !g_pServerDE || !LT1Mod::Script_Loaded()) return false;
	szClass[0] = '\0';
	HCLASS hClass = g_pServerDE->GetObjectClass(pPlayer->m_hObject);
	if (hClass) g_pServerDE->GetClassName(hClass, szClass, (uint32)nClass);
	o.hObj = pPlayer;
	o.pName = pName ? pName : "";
	o.pArchetype = "";
	o.pClass = szClass;
	return true;
}

} // namespace

void ModScript_PlayerSpawned(CBaseCharacter* p, const char* pName)
{
	LT1Mod::ScriptObject o; char c[64];
	if (FillPlayer(p, pName, o, c, sizeof(c))) LT1Mod::Script_PlayerSpawn(o);
}
void ModScript_PlayerDied(CBaseCharacter* p, const char* pName)
{
	LT1Mod::ScriptObject o; char c[64];
	if (FillPlayer(p, pName, o, c, sizeof(c))) LT1Mod::Script_PlayerDeath(o);
}
void ModScript_PlayerJoined(CBaseCharacter* p, const char* pName)
{
	LT1Mod::ScriptObject o; char c[64];
	if (FillPlayer(p, pName, o, c, sizeof(c))) LT1Mod::Script_PlayerJoin(o);
}
void ModScript_ClientMessage(CBaseCharacter* p, const char* pPlayerName, const char* pName, const char* pPayload)
{
	LT1Mod::ScriptObject o; char c[64];
	if (FillPlayer(p, pPlayerName, o, c, sizeof(c))) LT1Mod::Script_ClientMessage(o, pName, pPayload);
}
void ModScript_PlayerLeft(CBaseCharacter* p, const char* pName)
{
	LT1Mod::ScriptObject o; char c[64];
	if (FillPlayer(p, pName, o, c, sizeof(c))) LT1Mod::Script_PlayerLeave(o);
	else LT1Mod::Script_Removed(p);
}

void ModScript_WorldEnded()	{ LT1Mod::Script_WorldEnd(); }
void ModScript_Update()		{ LT1Mod::Script_Update(); }
