#include <string>

#include "ClientRes.h"
#include "BloodModStrings.h"
#include "lt1mod.h"
#include "lt1mod_strings.h"

namespace
{

#define SMS_STRING_ID(name) { #name, name },
const LT1Mod::StringName kNames[] =
{
#include "BloodModStringIds.inc"
};
#undef SMS_STRING_ID
const int kNumNames = sizeof(kNames) / sizeof(kNames[0]);

LT1Mod::StringTable g_Table(kNames, kNumNames, "Blood 2", "Blood2/ModReference/Strings.txt");

} // namespace

const char* BloodModString(int nId)					{ return g_Table.Get(nId); }
const char* BloodModString_Name(int nId)			{ return g_Table.Name(nId); }
int BloodModString_Id(const char* pName)			{ return g_Table.Id(pName); }
int BloodModString_NameCount()						{ return g_Table.NameCount(); }
const char* BloodModString_NameAt(int i)			{ return g_Table.NameAt(i); }
std::string BloodModString_Quote(const char* p)		{ return LT1Mod::StringTable::Quote(p); }
int BloodModString_ProbeId()						{ return g_Table.ProbeId(); }
void BloodModStrings_Clear()						{ g_Table.Clear(); }
bool BloodModStrings_Active()						{ return g_Table.Active(); }
std::string BloodModStrings_Summary()				{ return g_Table.Summary(); }

bool BloodModStrings_Load(LT1Mod::FileSource &src, std::string &sError)
{
	return g_Table.Load(src, sError);
}
