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

bool BloodModAI_Trace();
// The first call for each class answers true, once: the trace prints its constructor's values
// then
bool BloodModAI_FirstOfClass(const char* pClass);

// One line per class the mod sets, "CultistAI: HitPoints 60.0, RunSpeed 240.0"
std::string BloodModAI_Summary();
// The classes and keys, as an AI.txt: the reference file
std::string BloodModAI_Reference();

void BloodModAI_LoadAndReport(LT1Mod::FileSource &src, void (*pPrint)(const char *pLine), const char *pPrefix);

#endif // __BLOODMODAI_H__
