// Blood II's text strings, overridable by a mod's Mod\Strings.txt:
//
//     // anything after // is a comment
//     IDS_MENU_MAIN_SINGLEPLAYER = "Begin"
//     1234                       = "A string with no name, by its number"
//
// The client's FormatString answers with the mod's text for that id before the string module
// (CRes) is asked. The format and parser are lt1mod_strings.h's, which says the rest: the escapes,
// the 4000 character limit, and that a mod's string must keep the original's %1!s! inserts. The
// keys are Shared/ClientRes.h's IDS_ names, listed by the generated BloodModStringIds.inc
// (tools/shogo_string_ids.py --game blood2).
//
// The multiplayer announcer (TheVoice.cpp) takes each line's text from FormatString when it
// starts, after the mod has loaded; its lines are sprintf'd afterwards with the players' names, so
// retail writes them %%s, which the engine's formatting turns into %s, and a mod writes them so
// too. A Script object's SUBTITLEDISPLAY carries its own text. Only the client's FormatString asks;
// NetStart.cpp's LoadString calls, in the Windows-only network dialogs, do not.
//
// The reference is Blood2/ModReference/Strings.txt, every string the built CRes holds, generated
// by testBlood2Tables --dump strings.

#ifndef __BLOODMODSTRINGS_H__
#define __BLOODMODSTRINGS_H__

#include <string>

namespace LT1Mod { class FileSource; }

// The mod's text for a string id, or NULL
const char* BloodModString(int nId);

const char* BloodModString_Name(int nId);
int BloodModString_Id(const char* pName);
int BloodModString_NameCount();
const char* BloodModString_NameAt(int i);
std::string BloodModString_Quote(const char* pText);
int BloodModString_ProbeId();

bool BloodModStrings_Load(LT1Mod::FileSource &src, std::string &sError);
void BloodModStrings_Clear();
bool BloodModStrings_Active();
std::string BloodModStrings_Summary();

#endif // __BLOODMODSTRINGS_H__
