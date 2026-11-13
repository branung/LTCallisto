// Blood II's AI constants, overridable by a mod's Mod\AI.txt:
//
//     [CultistAI]
//     HitPoints = 60.0
//     RunSpeed  = 240.0
//
//     [Options]
//     Trace = TRUE
//
// Every CultistAI created afterwards starts with those values.
//
// Not a table: each AI class sets its constants in its own constructor, so there is no array to
// write in place. Instead AI_Mgr's MID_PRECREATE, which every AI class chains to first, calls
// BloodModAI_Apply with the class's name after the constructor has run and before
// AI_Mgr::ReadProp, so the mod's value replaces the constructor's and a level's own property still
// replaces the mod's. For a world file object, a key its class declares as a property (CultistAI
// declares RandomHitPoints, AIState, AIWeapon1, Bullets and Flares) therefore changes nothing,
// because DEdit saves every declared property; it does change an AI made by code, and every other
// key changes both. What the game does with the values afterwards is unchanged: InitialUpdate
// scales hit points and armor by the difficulty, and a divine ZealotAI doubles its hit points.
//
// A section is an AI class's registered name, as a level names it (CultistAI, ShikariAI,
// Behemoth, ...), without case; BloodModAI.cpp's kClasses lists the 32 (34 with the Nightmare
// Levels), and tests/Blood2Tables checks that list against every END_CLASS_DEFAULT in ObjectDLL
// whose parent is AI_Mgr. A key is a member, listed in BloodModAIFields.h; where a level property
// sets the same member the key is the property's name. AIState and AIWeapon1-5 are read by the
// class's own StateStrToInt and WeaponStrToInt, as a level's are, so only their length (31) is
// checked here.
//
// [Options] Trace = TRUE prints, the first time each class is created, the values its constructor
// gave it, and then each object the mod changed.
//
// A section that names no class is an archetype, a new enemy type, and names its Base: a class, or
// another archetype in the file:
//
//     [RedCultist]
//     Base      = "CultistAI"
//     HitPoints = 120.0
//     AIWeapon1 = "SHOTGUN"
//     Skin      = "Skins\Enemies\m_Cultist3.dtx"
//
// An AI of the root class with the property Archetype = "RedCultist" (every AI class declares it,
// through AI_Mgr) is one: a level places it, and a Spawner makes one from
// "CultistAI Archetype RedCultist". Its values apply after the class's own section, root first,
// and before the level's own properties, which still win. Skin replaces the skin the class picks
// at MID_PRECREATE, at MID_INITIALUPDATE; a class section may give one too. There is no Model key:
// each class keeps its animations' indexes in a static shared by every instance, set from the
// first one's model, so a second model could only be one with the same animations in the same
// order.

#ifndef __BLOODMODAI_H__
#define __BLOODMODAI_H__

#include <string>

#include "BloodModAIFields.h"

namespace LT1Mod { class FileSource; }

// The AI class names, without case; NULL past the last
const char* BloodModAI_Class(int n);
int BloodModAI_FindClass(const char* pName);	// its index, or -1

// The keys, in BloodModAIFields.h's order; NULL past the last
const char* BloodModAI_Key(int nField);
// One key of a set of values, as AI.txt writes it ("60.0", "5", "\"IDLE\"")
std::string BloodModAI_Value(const BloodModAIValues& v, int nField);

// Loads Mod\AI.txt. Clears what a load before set first; an absent file, or no active mod, is no
// overrides and no error. A bad file (a section that names no AI class, an unknown key, a value
// of the wrong type or out of range) loads nothing and returns false with sError saying where.
bool BloodModAI_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModAI_Clear();

// Writes the mod's values for class pClass (a registered class name) into v. Returns the number
// of keys written; 0 when the mod sets nothing for it.
int BloodModAI_Apply(const char* pClass, BloodModAIValues& v);

// Archetypes: the index of a name, or -1; its name and the class at the root of its chain; its
// values, written into v root first; NULL for no index
int BloodModAI_FindArchetype(const char* pName);
const char* BloodModAI_ArchetypeName(int nArch);
const char* BloodModAI_ArchetypeClass(int nArch);
int BloodModAI_ApplyArchetype(int nArch, BloodModAIValues& v);
// The skin a mod gives an AI of class pClass, as archetype nArch (-1 for none): its chain's
// nearest, else its class section's; NULL for none
const char* BloodModAI_Skin(const char* pClass, int nArch);

bool BloodModAI_Trace();
// [Options] Script = TRUE: every AI class's AIs are heard by the mod's script, as a class
// section's Script = TRUE makes one class's
bool BloodModAI_ScriptAll();

// The death table's [Options] half, for every class; a class's own keys are BloodModAIValues
// members. nKind is a BLOODMOD_DEATH_KINDS index. DeathSkin answers the game's own where the mod
// says nothing (fire: burned.dtx), "" for none; DeathFX "" for none.
const char* BloodModAI_DeathSkin(int nKind);
const char* BloodModAI_DeathFX(int nKind);
bool BloodModAI_BulletGib();			// [Options] BulletGib, TRUE unless the mod says
// An OBJFX_ name without its prefix, any case -> its id; -1 for none.
int BloodModAI_ObjFX(const char* pName);

// [Options] DifficultyDamage, DifficultyHitPoints and DifficultyArmor, each <easy, medium, hard>:
// the scales AI_Mgr::InitialUpdate applies by difficulty. The scale for nDifficulty (0 easy, 1
// medium, 2 hard) of nKind, or fRetail when the mod sets none, and always for insane
enum BloodModAI_DifficultyKind { BLOODMOD_DIFFICULTY_DAMAGE, BLOODMOD_DIFFICULTY_HITPOINTS, BLOODMOD_DIFFICULTY_ARMOR,
								 BLOODMOD_DIFFICULTY_KINDS };
float BloodModAI_DifficultyScale(int nKind, int nDifficulty, float fRetail);
// The first call for each class answers true, once: the trace prints its constructor's values
// then
bool BloodModAI_FirstOfClass(const char* pClass);

// One line per class the mod sets, "CultistAI: HitPoints 60.0, RunSpeed 240.0"
std::string BloodModAI_Summary();
// The classes and keys, as an AI.txt: the reference file
std::string BloodModAI_Reference();

void BloodModAI_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODAI_H__
