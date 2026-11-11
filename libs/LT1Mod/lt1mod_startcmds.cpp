#include <stdio.h>
#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include "lt1mod_startcmds.h"
#include "lt1mod.h"
#include "lt1mod_attrib.h"

namespace LT1Mod
{

namespace
{

bool g_bLoaded = false;

// ButeMgr's own string bound, stated here so the error names the file
const size_t kMaxStart = 4000;

std::map<std::string, std::string> g_Start;		// lower world name, or "" for [Global]
bool g_bTrace = false;
std::string g_Summary;

std::string Lower(const std::string& s)
{
	std::string r(s);
	for (size_t i = 0; i < r.size(); ++i)
		r[i] = (char)tolower((unsigned char)r[i]);
	return r;
}

// "Worlds\01_Ambush.dat" -> "01_ambush"
std::string WorldKey(const char* p)
{
	std::string s(p ? p : "");
	const size_t slash = s.find_last_of("\\/");
	if (slash != std::string::npos) s = s.substr(slash + 1);
	const size_t dot = s.find_last_of('.');
	if (dot != std::string::npos) s = s.substr(0, dot);
	return Lower(s);
}

} // namespace

std::vector<std::string> StartCommands_For(const char* pWorld)
{
	std::vector<std::string> out;
	if (!g_bLoaded) return out;
	std::map<std::string, std::string>::const_iterator it = g_Start.find("");
	if (it != g_Start.end()) out.push_back(it->second);
	it = g_Start.find(WorldKey(pWorld));
	if (pWorld && it != g_Start.end()) out.push_back(it->second);
	return out;
}

bool StartCommands_Trace() { return g_bLoaded && g_bTrace; }

bool StartCommands_Loaded() { return g_bLoaded; }

void StartCommands_Clear()
{
	g_Start.clear();
	g_bTrace = false;
	g_Summary.clear();
	g_bLoaded = false;
}

std::string StartCommands_Summary() { return g_Summary; }

bool StartCommands_Load(FileSource &src, std::string &sError)
{
	StartCommands_Clear();
	sError.clear();
	if (!IsActive())
		return true;

	const char* const kPath = "Mod\\Commands.txt";
	std::string text;
	if (!src.ReadFile(kPath, text))
		return true;

	Attributes a;
	if (!a.Parse(text.data(), text.size(), kPath))
	{
		sError = a.Error();
		return false;
	}

	char buf[512];
	std::map<std::string, std::string> start;
	bool bTrace = false;
	std::string summary;
	const std::vector<std::string>& tags = a.Tags();
	for (size_t t = 0; t < tags.size(); ++t)
	{
		const char* tag = tags[t].c_str();
		const std::string ltag = Lower(tags[t]);
		const std::vector<std::string> keys = a.Keys(tag);
		for (size_t k = 0; k < keys.size(); ++k)
		{
			const std::string lkey = Lower(keys[k]);
			const std::string before = a.Error();
			if (ltag == "commands")
			{
				if (lkey != "trace")
				{
					snprintf(buf, sizeof(buf), "%s: [Commands] has no key \"%s\" (Trace)",
							 kPath, keys[k].c_str());
					sError = buf;
					return false;
				}
				bTrace = a.GetBool(tag, keys[k].c_str(), false);
			}
			else if (ltag != "global" && ltag.compare(0, 6, "world_") != 0)
			{
				snprintf(buf, sizeof(buf), "%s: [%s] is not [Commands], [Global] or a world's [World_<name>]", kPath, tag);
				sError = buf;
				return false;
			}
			else if (lkey == "start")
			{
				const std::string s = a.GetString(tag, keys[k].c_str(), "");
				if (a.Error() == before && s.size() > kMaxStart)
				{
					snprintf(buf, sizeof(buf), "%s: [%s]:Start is longer than %u characters", kPath, tag, (unsigned)kMaxStart);
					sError = buf;
					return false;
				}
				start[ltag == "global" ? std::string() : ltag.substr(6)] = s;
				summary += (summary.empty() ? "" : "\n") + std::string(tag) + ": Start \"" + s + "\"";
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s: [%s] has no key \"%s\" (a world's section and [Global] take Start)", kPath, tag, keys[k].c_str());
				sError = buf;
				return false;
			}
			if (a.Error() != before)
			{
				sError = std::string(kPath) + ": " + a.Error();
				return false;
			}
		}
	}
	if (bTrace) summary = std::string("Trace on") + (summary.empty() ? "" : "\n") + summary;

	g_Start.swap(start);
	g_bTrace = bTrace;
	g_Summary = summary;
	g_bLoaded = true;
	return true;
}

} // namespace LT1Mod
