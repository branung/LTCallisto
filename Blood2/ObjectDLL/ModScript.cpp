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
#include "SFXMsgIds.h"			// mod.explode: EXP_, SFX_EXPLOSIONFX_ID
#include "SoundTypes.h"
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <vector>

#include <string>

namespace
{

// The game's DAMAGE_TYPE_ values (SharedDefs.h), as a script names them.
const char* DamageKind(int nType)
{
	static const char* const kKinds[] = { "NORMAL", "FIRE", "FREEZE", "SINGULARITY", "MELEE", "EXPLODE", "SUFFOCATE",
										  "DEATH", "FREEFALL", "BLIND", "SLOW", "DROPWEAPON", "VISION", "ELECTRIC",
										  "ACID", "LEECH" };
	if (nType < 0) return DNULL;
	nType &= 0x0f;		// the type, without its flag bits
	return kKinds[nType];
}

// mod.explode's effect names: SFXMsgIds.h's EXP_, without the prefix.
struct Effect { const char* pName; DDWORD dwId; };
#define E(n) { #n, EXP_##n }
const Effect kEffects[] =
{
	E(DEFAULT_SMALL), E(DEFAULT_MEDIUM), E(DEFAULT_LARGE), E(GRENADE), E(NAPALM_PRIMARY), E(NAPALM_ALT),
	E(NAPALM_FIREBALL), E(TESLA_PRIMARY), E(TESLA_ALT), E(HOWITZER_PRIMARY), E(HOWITZER_ALT), E(HOWITZER_MINI),
	E(FLAME_SMALL), E(FLARE_FIZZLE), E(FLARE_BURST), E(FLARE_FRAG), E(LIFELEECH_PRIMARY), E(LIFELEECH_ALT),
	E(SINGULARITY_PRIMARY), E(SHOCKBALL), E(SHOCKBALL_LARGE), E(ORB_BREAK), E(DIVINE_SHOCKBALL), E(RIFT_1),
	E(RIFT_2), E(DEATHRAY_PRIMARY), E(BUGSPRAY_PRIMARY), E(BUGSPRAY_ALT), E(NAGA_EYE_BEAM), E(NAGA_SPIKE),
	E(NAGA_STONE_CHUNK), E(NAGA_POUND_GROUND),
#ifdef _ADD_ON
	E(GAS_GRENADE), E(FLAYER_PRIMARY), E(FLAYER_ALT), E(FLAYER_RETRACT), E(FLAYER_SHATTER),
#endif
};
#undef E

bool SameNoCase(const char* a, const char* b)
{
	for (; *a && *b; ++a, ++b)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
	return *a == *b;
}

// The script's handle for an object: the CBaseCharacter it is, or NULL.
void* ScriptHandle(HOBJECT h)
{
	if (!h || !g_pServerDE) return NULL;
	HCLASS hChar = g_pServerDE->GetClass("CBaseCharacter");
	if (!hChar || !g_pServerDE->IsKindOf(g_pServerDE->GetObjectClass(h), hChar)) return NULL;
	return static_cast<CBaseCharacter*>(g_pServerDE->HandleToObject(h));
}

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
	// Version 6: mod.explode. The effect every client draws, then each model and world model within
	// the radius, except the source, takes a random min..max of DAMAGE_TYPE_EXPLODE
	virtual bool Explode(float x, float y, float z, float fRadius, float fMin, float fMax, const char* pEffect,
						 void* hSource, std::string& sWhy)
	{
		if (!g_pServerDE) { sWhy = "no server"; return false; }
		CBaseCharacter* pSource = static_cast<CBaseCharacter*>(hSource);
		HOBJECT hSrc = pSource ? pSource->m_hObject : DNULL;
		DVector vPos, vNormal;
		VEC_SET(vPos, x, y, z);
		VEC_SET(vNormal, 0.0f, 1.0f, 0.0f);
		if (pEffect && pEffect[0])
		{
			const Effect* pE = DNULL;
			for (size_t i = 0; i < sizeof(kEffects) / sizeof(kEffects[0]) && !pE; ++i)
				if (SameNoCase(kEffects[i].pName, pEffect)) pE = &kEffects[i];
			if (!pE) { sWhy = std::string("no explosion \"") + pEffect + "\" (SFXMsgIds.h's EXP_ names, without EXP_)"; return false; }
			HMESSAGEWRITE hMessage = g_pServerDE->StartInstantSpecialEffectMessage(&vPos);
			g_pServerDE->WriteToMessageByte(hMessage, SFX_EXPLOSIONFX_ID);
			g_pServerDE->WriteToMessageVector(hMessage, &vPos);
			g_pServerDE->WriteToMessageVector(hMessage, &vNormal);
			g_pServerDE->WriteToMessageDWord(hMessage, pE->dwId);
			g_pServerDE->EndMessage(hMessage);
		}
		ObjectList* ol = g_pServerDE->FindObjectsTouchingSphere(&vPos, fRadius);
		if (!ol) return true;
		// The list first, the damage after: damage can remove an object.
		std::vector<HOBJECT> hit;
		for (ObjectLink* pLink = ol->m_pFirstLink; pLink; pLink = pLink->m_pNext)
		{
			const short t = (short)g_pServerDE->GetObjectType(pLink->m_hObject);
			if (pLink->m_hObject != hSrc && (t == OT_MODEL || t == OT_WORLDMODEL)) hit.push_back(pLink->m_hObject);
		}
		g_pServerDE->RelinquishList(ol);
		for (size_t i = 0; i < hit.size(); ++i)
		{
			DVector vObj, vDir;
			g_pServerDE->GetObjectPos(hit[i], &vObj);
			VEC_SUB(vDir, vObj, vPos);
			const DFLOAT fDamage = fMax > fMin ? g_pServerDE->Random(fMin, fMax) : fMin;
			// DamageObject sends nothing without a damager, so with no source each object is its own, as a
			// script's obj:damage is.
			DEBaseClass* pDamager = pSource ? (DEBaseClass*)pSource : (DEBaseClass*)g_pServerDE->HandleToObject(hit[i]);
			if (!pDamager) continue;
			DamageObject(hSrc ? hSrc : hit[i], pDamager, hit[i], fDamage, vDir, vObj, DAMAGE_TYPE_EXPLODE);
		}
		return true;
	}
	// Version 4: one of the mod's archetypes, created at a point.  By its own
	// class where there is one, else by its host with the Archetype
	// property, as a Spawner's string does.
	virtual void* Spawn(const char* pArchetype, float x, float y, float z, const char* pProps, std::string& sWhy)
	{
		bool bObject = false;
		return SpawnEx(pArchetype, x, y, z, pProps, 0.0f, bObject, sWhy);
	}
	// Version 6: an archetype as before, or any class the game registers (a level could place it
	// too), turned fYaw degrees about the vertical. Only an archetype is an object the script holds
	// by right; a retail AI the mod opted in is one through its own OnCreate (ModScript_Created).
	virtual void* SpawnEx(const char* pArchetype, float x, float y, float z, const char* pProps, float fYaw, bool& bObject,
						  std::string& sWhy)
	{
		bObject = false;
		if (!g_pServerDE) { sWhy = "no server"; return NULL; }
		const int nArch = BloodModAI_FindArchetype(pArchetype);
		std::string props = pProps ? pProps : "";
		HCLASS hClass = g_pServerDE->GetClass((char*)pArchetype);
		if (nArch >= 0)
		{
			bObject = true;
			if (!hClass)
			{
				hClass = g_pServerDE->GetClass((char*)BloodModAI_ArchetypeClass(nArch));
				props = std::string("Archetype ") + pArchetype + (props.empty() ? "" : " ") + props;
			}
		}
		else if (!hClass) { sWhy = "no such archetype in Mod\\AI.txt and no such class"; return NULL; }
		if (!hClass) { sWhy = "no class to make it with"; return NULL; }
		ObjectCreateStruct ocs;
		INIT_OBJECTCREATESTRUCT(ocs);
		VEC_SET(ocs.m_Pos, x, y, z);
		g_pServerDE->SetupEuler(&ocs.m_Rotation, 0.0f, fYaw * (float)(MATH_PI / 180.0), 0.0f);
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
	// "" is a retail AI the mod opted in; NULL is no event.
	if (!pChar || !pChar->m_hObject || !pArchetype || !g_pServerDE || !LT1Mod::Script_Loaded())
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

void ModScript_Created(CBaseCharacter* pChar, const char* pArchetype, DBOOL bPlaced)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass))) LT1Mod::Script_Create(o, bPlaced ? true : false);
}

void ModScript_Damaged(CBaseCharacter* pChar, const char* pArchetype, DFLOAT fAmount, HOBJECT hFrom, int nType)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass)))
		LT1Mod::Script_Damage(o, fAmount, ScriptHandle(hFrom), DamageKind(nType));
}

void ModScript_Died(CBaseCharacter* pChar, const char* pArchetype, HOBJECT hKiller, int nType, DFLOAT fDamage)
{
	LT1Mod::ScriptObject o;
	char szClass[64];
	if (Fill(pChar, pArchetype, o, szClass, sizeof(szClass)))
		LT1Mod::Script_Death(o, ScriptHandle(hKiller), DamageKind(nType), fDamage);
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
