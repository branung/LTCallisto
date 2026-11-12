// The client shell's answer to the engine's string lookup.
// CLTClient::FormatString finds this function by name and asks it first, so a mod's
// Mod\Strings.txt reaches every FormatString call. NULL means "not mine" and the
// engine then asks CRes.

#include "ltbasedefs.h"
#include "BloodModStrings.h"

extern "C" MODULE_EXPORT const char* LT1Mod_ClientString(int nId)
{
	return BloodModString(nId);
}
