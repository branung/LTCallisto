#include "server_de.h"
#include "WeaponDefs.h"
#include "BloodModWeapons.h"
#include "BloodModTable.h"

#include <stddef.h>
#include <string>

using namespace BloodMod;

namespace
{

// The WEAP_ names, by id (SharedDefs.h). _ADD_ON renumbers the table and adds four;
// tests/Blood2Tables checks that every id from 1 to WEAP_MAXWEAPONTYPES has exactly one name.
#define W(x) { WEAP_##x, #x }
const Entry kWeaponNames[] =
{
	W(BERETTA), W(SUBMACHINEGUN), W(FLAREGUN), W(SHOTGUN), W(SNIPERRIFLE), W(HOWITZER),
	W(NAPALMCANNON), W(SINGULARITY), W(ASSAULTRIFLE), W(BUGSPRAY), W(MINIGUN), W(DEATHRAY),
	W(TESLACANNON), W(VOODOO), W(ORB), W(LIFELEECH), W(MELEE),
#ifdef _ADD_ON
	W(COMBATSHOTGUN), W(FLAYER),
#endif
	W(SHIKARI_CLAW), W(SHIKARI_SPIT), W(SOUL_CROWBAR), W(SOUL_AXE), W(SOUL_PIPE), W(SOUL_HOOK),
	W(BEHEMOTH_CLAW), W(ZEALOT_HEAL), W(ZEALOT_SHIELD), W(ZEALOT_ENERGYBLAST), W(ZEALOT_GROUNDFIRE),
	W(ZEALOT_SHOCKWAVE), W(DRUDGE_FIREBALL), W(DRUDGE_LIGHTNING), W(HAND_SQUEEZE), W(BONELEECH_SUCK),
	W(THIEF_SUCK), W(NIGHTMARE_BITE), W(BEHEMOTH_SHOCKWAVE), W(DEATHSHROUD_ZAP), W(NAGA_EYEBEAM),
	W(NAGA_SPIKE), W(NAGA_DEBRIS), W(GIDEON_SHIELD), W(GIDEON_WIND), W(GIDEON_VOMIT), W(GIDEON_GOO),
	W(GIDEON_SPEAR), W(ANCIENTONE_BEAM), W(ANCIENTONE_TENTACLE), W(SKULL),
#ifdef _ADD_ON
	W(GREMLIN_ROCK), W(NIGHTMARE_FIREBALLS),
#endif
	W(PROXIMITYBOMB), W(REMOTEBOMB), W(TIMEBOMB),
};
#undef W
const int kNumNames = (int)(sizeof(kWeaponNames) / sizeof(kWeaponNames[0]));

// A byte field's names and values
const Named kFireTypes[] =
{
	{ "RIFLE", TYPE_RIFLE }, { "AUTORIFLE", TYPE_AUTORIFLE }, { "PISTOL", TYPE_PISTOL },
	{ "MELEE", TYPE_MELEE }, { "MAGIC", TYPE_MAGIC }, { 0, 0 }
};
const Named kAmmoTypes[] =
{
	{ "NONE", AMMO_NONE }, { "BULLET", AMMO_BULLET }, { "SHELL", AMMO_SHELL }, { "BMG", AMMO_BMG },
	{ "FLARE", AMMO_FLARE }, { "DIEBUGDIE", AMMO_DIEBUGDIE }, { "HOWITZER", AMMO_HOWITZER },
	{ "FUEL", AMMO_FUEL }, { "BATTERY", AMMO_BATTERY }, { "FOCUS", AMMO_FOCUS },
	{ "PROXIMITYBOMB", AMMO_PROXIMITYBOMB }, { "REMOTEBOMB", AMMO_REMOTEBOMB },
	{ "TIMEBOMB", AMMO_TIMEBOMB }, { 0, 0 }
};

#define F(key, member, kind) { key, 0, offsetof(WeaponData, member), kind, 0 }
#define FN(key, member, names) { key, 0, offsetof(WeaponData, member), K_NAMED, names }
#define FS(key, member, part) { key, 0, offsetof(WeaponData, member) + offsetof(Spread, part), K_FLOAT, 0 }
// In WeaponData's order, so the reference file reads as WeaponDefs.cpp does
const Field kFields[] =
{
	F("ViewModel",			m_szViewModelFilename,		K_STRING),
	F("LeftViewModel",		m_szLeftViewModelFilename,	K_STRING),
	F("ViewSkin",			m_szViewModelSkin,			K_STRING),
	F("HandModel",			m_szHandModelFilename,		K_STRNULL),
	F("HandSkin",			m_szHandModelSkin,			K_STRNULL),
	FN("FireType",			m_nFireType,				kFireTypes),
	FN("AmmoType",			m_nAmmoType,				kAmmoTypes),
	F("AmmoUse",			m_nAmmoUse,					K_WORD),
	F("AltAmmoUse",			m_nAltAmmoUse,				K_WORD),
	F("MinDamage",			m_fMinDamage,				K_FLOAT),
	F("MaxDamage",			m_fMaxDamage,				K_FLOAT),
	F("MinAltDamage",		m_fMinAltDamage,			K_FLOAT),
	F("MaxAltDamage",		m_fMaxAltDamage,			K_FLOAT),
	F("ReloadTime",			m_fReloadTime,				K_FLOAT),
	F("AltReloadTime",		m_fAltReloadTime,			K_FLOAT),
	FS("SpreadH",			m_Spread,					h),
	FS("SpreadV",			m_Spread,					v),
	FS("AltSpreadH",		m_AltSpread,				h),
	FS("AltSpreadV",		m_AltSpread,				v),
	F("ProjVelocity",		m_fProjVelocity,			K_FLOAT),
	F("AltProjVelocity",	m_fAltProjVelocity,			K_FLOAT),
	F("Range",				m_fRange,					K_FLOAT),
	F("AltRange",			m_fAltRange,				K_FLOAT),
	F("ShotsPerFire",		m_dwShotsPerFire,			K_DWORD),
	F("AltShotsPerFire",	m_dwAltShotsPerFire,		K_DWORD),
	F("StrengthReq",		m_dwStrengthReq,			K_DWORD),
	F("TwoHandStrengthReq",	m_dwTwoHandStrengthReq,		K_DWORD),
	F("DamageRadius",		m_nDamageRadius,			K_INT),
	F("AltDamageRadius",	m_nAltDamageRadius,			K_INT),
	F("AltFireZoom",		m_bAltFireZoom,				K_BOOL),
	F("SemiAuto",			m_bSemiAuto,				K_BOOL),
	F("FireSound",			m_szFireSound,				K_STRING),
	F("AltFireSound",		m_szAltFireSound,			K_STRING),
	F("EmptySound",			m_szEmptyWeaponSound,		K_STRING),
	F("AltEmptySound",		m_szAltEmptyWeaponSound,	K_STRING),
	F("ProjectileClass",	m_szProjectileClass,		K_STRNULL),
	F("AltProjectileClass",	m_szAltProjectileClass,		K_STRNULL),
	F("FlashRadius",		m_fFlashRadius,				K_FLOAT),
	F("FlashColor",			m_vFlashColor,				K_VECTOR),
	F("Name",				m_szWeaponName,				K_STRING),
	F("NameID",				m_nWeaponNameID,			K_INT),
	F("FlashSprite",		m_szFlashSprite,			K_STRING),
	F("AltFlashSprite",		m_szAltFlashSprite,			K_STRING),
	F("FlashDuration",		m_fFlashDuration,			K_FLOAT),
	F("FlashScale",			m_fFlashScale,				K_FLOAT),
	F("HandOffset",			m_vHandModelOffset,			K_VECTOR),
	F("ViewOffset",			m_vViewModelOffset,			K_VECTOR),
	F("MuzzleOffset",		m_vMuzzleOffset,			K_VECTOR),
	F("Recoil",				m_vRecoil,					K_VECTOR),
	F("FlashOffset",		m_vFlash,					K_VECTOR),
	F("EjectInterval",		m_fEjectInterval,			K_FLOAT),
	F("ViewKick",			m_fViewKick,				K_FLOAT),
	F("CumulativeKick",		m_bCumulativeKick,			K_BOOL),
	F("LoopAnim",			m_bLoopAnim,				K_BOOL),
	F("AltLoopAnim",		m_bAltLoopAnim,				K_BOOL),
	F("StatusIcon",			m_szStatusIcon,				K_STRNULL),
	F("StatusIconH",		m_szStatusIconH,			K_STRNULL),
};
#undef F
#undef FN
#undef FS
const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

const Array kArrays[] = { { g_WeaponDefaults, sizeof(WeaponData) } };

// Ids 1 .. WEAP_MAXWEAPONTYPES, element id - 1. A section naming no weapon adds one on a Base, up
// to id BLOOD2_WEAPON_IDS.
Table g_Table("Mod\\Weapons.txt", "a Blood 2 weapon", "the WEAP_ names without the prefix",
			  "Blood2/ModReference/Weapons.txt", kArrays, 1, WEAP_MAXWEAPONTYPES, 1,
			  kWeaponNames, kNumNames, kFields, kNumFields, BLOOD2_WEAPON_IDS, "Base", "weapon");

} // namespace

const char* BloodModWeapon_Name(int nId)						{ return g_Table.Name(nId); }
int BloodModWeapon_Find(const char* pName)						{ return g_Table.Find(pName); }
const char* BloodModWeapon_Key(int nField)						{ return g_Table.Key(nField); }
std::string BloodModWeapon_Value(int nId, int nField, bool bRetail)	{ return g_Table.Value(nId, nField, bRetail); }
int BloodModWeapon_Count()										{ return g_Table.Total(); }
int BloodModWeapon_Base(int nId)								{ const int n = g_Table.Base(nId); return n > 0 ? n : 0; }
int BloodModWeapon_Retail(int nId)								{ return g_Table.Retail(nId); }
void BloodModWeapons_Clear()									{ g_Table.Clear(); }
bool BloodModWeapons_Load(LT1Mod::FileSource &src, std::string &sError)
{
	if (!g_Table.Load(src, sError)) return false;
	// The three bombs are inventory items, which ObtainWeapon never makes (it sends
	// WEAP_BASEINVENTORY and up to AddInventoryWeapon), so a new weapon on one could never be
	// held. Refused, and nothing loaded.
	for (int nId = (WEAP_MAXWEAPONTYPES) + 1; nId <= g_Table.Total(); ++nId)
		if (g_Table.Retail(nId) >= WEAP_BASEINVENTORY)
		{
			sError = std::string("Mod\\Weapons.txt: [") + g_Table.Name(nId) + "] is built on " +
					 g_Table.Name(g_Table.Retail(nId)) + ", a bomb: bombs are inventory items, and a new weapon cannot be one";
			g_Table.Clear();
			return false;
		}
	return true;
}
std::string BloodModWeapons_Summary()							{ return g_Table.Summary(); }
std::string BloodModWeapons_Reference()							{ return g_Table.Reference(); }

void BloodModWeapons_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix)
{
	g_Table.LoadAndReport(src, pPrint, pPrefix);
}
