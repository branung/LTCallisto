// testCommands: LT1Mod's CommandMgr port against NOLF1's original. Compiles the original beside
// it and replays the same scripts through both, under one recorded engine (the engine's own
// tokenizer, one seeded random stream, a log of every message sent and line printed). The port
// is faithful when the two logs are equal, frame for frame.
//
//   testCommands --selftest
//
// 1. known answers, run through the port alone, so the port is right and not merely like NOLF1
// 2. the replay: hand written edge cases and 400 generated scripts, port against original
// 3. calibration: the comparison catches a one character change

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <string>
#include <vector>

#include "Stdafx.h"				// stub/: the recorded engine NOLF1's file sees
#include "CommandMgr.h"			// NOLF1's, copied as shipped
#include "conparse.h"			// the engine's cp_Parse, copied as shipped

#include "lt1mod_command.h"		// the port

// The recorded engine

namespace
{

std::vector<std::string> g_Log;
uint32_t g_nRand = 1;

uint32_t Next()
{
	g_nRand = g_nRand * 1103515245u + 12345u;
	return (g_nRand >> 8) & 0xffffff;
}

void Log(const std::string& s) { g_Log.push_back(s); }

} // namespace

int GetRandom(int lo, int hi)
{
	if (hi <= lo) return lo;
	return lo + (int)(Next() % (uint32_t)(hi - lo + 1));
}

float GetRandom(float lo, float hi)
{
	return lo + (hi - lo) * ((float)Next() / (float)0xffffff);
}

LTBOOL SendTriggerMsgToObjects(LPBASECLASS, const char* pName, const char* pMsg)
{
	Log(std::string("SEND ") + pName + " <- " + pMsg);
	return LTTRUE;
}

StubServer g_Server;
StubServer* g_pLTServer = &g_Server;

void StubServer::CPrint(const char* pMsg, ...)
{
	char buf[1024];
	va_list v;
	va_start(v, pMsg);
	vsnprintf(buf, sizeof(buf), pMsg, v);
	va_end(v);
	Log(std::string("PRINT ") + buf);
}

namespace
{

class Host : public LT1Mod::CommandHost
{
public:
	int Parse(const char* pCommand, const char** pNewCommandPos, char* argBuffer, const char** argPointers, int* nArgs)
	{
		return cp_Parse(pCommand, pNewCommandPos, argBuffer, argPointers, nArgs);
	}
	void SendMessage(const char* pName, const char* pMsg) { SendTriggerMsgToObjects(LTNULL, pName, pMsg); }
	int RandomInt(int lo, int hi) { return GetRandom(lo, hi); }
	float RandomFloat(float lo, float hi) { return GetRandom(lo, hi); }
	void Print(const char* pLine) { Log(std::string("PRINT ") + pLine); }
	// SCRIPT: logged, and answered by every name but "nobody"
	bool RunScript(const char* pName, const char* const* pArgs, int nArgs)
	{
		std::string line = std::string("SCRIPT ") + pName;
		for (int i = 0; i < nArgs; ++i) line += std::string("|") + pArgs[i];
		Log(line);
		return strcmp(pName, "nobody") != 0;
	}
};

int g_nPass = 0, g_nFail = 0;

void Check(bool b, const std::string& what)
{
	printf("  %s  %s\n", b ? "ok  " : "FAIL", what.c_str());
	(b ? g_nPass : g_nFail)++;
}

// A run: the script processed at time 0, then the frames, each logged
struct Run
{
	std::string			script;
	std::vector<float>	frames;
};

std::vector<float> Frames(int n, float dt)
{
	return std::vector<float>((size_t)n, dt);
}

// Uneven frames, from their own stream so the script's draws are untouched
std::vector<float> RaggedFrames(int n, uint32_t seed)
{
	std::vector<float> v;
	for (int i = 0; i < n; ++i)
	{
		seed = seed * 1664525u + 1013904223u;
		v.push_back(0.005f + 0.2f * (float)((seed >> 8) & 0xffff) / 65535.0f);
	}
	return v;
}

std::vector<std::string> RunPort(const Run& r, uint32_t seed, LT1Mod::CommandMgr** ppKeep = 0)
{
	static Host host;
	g_Log.clear();
	g_nRand = seed;
	LT1Mod::CommandMgr* p = new LT1Mod::CommandMgr(host);
	Log(std::string("RET ") + (p->Process(r.script.c_str()) ? "1" : "0"));
	for (size_t f = 0; f < r.frames.size(); ++f)
	{
		Log("FRAME " + std::to_string(f));
		p->Update(r.frames[f]);
	}
	if (ppKeep) *ppKeep = p; else delete p;
	return g_Log;
}

std::vector<std::string> RunNolf(const Run& r, uint32_t seed)
{
	g_Log.clear();
	g_nRand = seed;
	CCommandMgr* p = new CCommandMgr();
	Log(std::string("RET ") + (p->Process(r.script.c_str()) ? "1" : "0"));
	for (size_t f = 0; f < r.frames.size(); ++f)
	{
		Log("FRAME " + std::to_string(f));
		g_Server.fFrameTime = r.frames[f];
		p->Update();
	}
	delete p;
	return g_Log;
}

std::string FirstDiff(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
	for (size_t i = 0; i < a.size() && i < b.size(); ++i)
		if (a[i] != b[i]) return "line " + std::to_string(i) + ": port \"" + a[i] + "\" vs NOLF1 \"" + b[i] + "\"";
	if (a.size() != b.size()) return "port has " + std::to_string(a.size()) + " lines, NOLF1 " + std::to_string(b.size());
	return std::string();
}

std::string Joined(const std::vector<std::string>& v, bool bFramesToo)
{
	std::string s;
	for (size_t i = 0; i < v.size(); ++i)
		if (bFramesToo || v[i].compare(0, 6, "FRAME ") != 0)
			s += (s.empty() ? "" : " | ") + v[i];
	return s;
}

// The frame a line was logged in (-1 before the first)
int FrameOf(const std::vector<std::string>& v, const std::string& line)
{
	int f = -1;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i].compare(0, 6, "FRAME ") == 0) f = atoi(v[i].c_str() + 6);
		else if (v[i] == line) return f;
	}
	return -2;
}

int Count(const std::vector<std::string>& v, const std::string& line)
{
	int n = 0;
	for (size_t i = 0; i < v.size(); ++i) n += v[i] == line;
	return n;
}

// 1. Known answers

void TestKnown()
{
	printf("known answers (the port alone)\n");
	std::vector<std::string> v;

	Run r;
	r.script = "MSG door1 TRIGGER";
	v = RunPort(r, 1);
	Check(Joined(v, true) == "SEND door1 <- TRIGGER | RET 1", "MSG sends at once, before Process returns: " + Joined(v, true));

	r.script = "MSG (door1 door2) (LOCK 3); MSG lamp ON";
	v = RunPort(r, 1);
	Check(Joined(v, true) == "SEND door1 <- LOCK 3 | SEND door2 <- LOCK 3 | SEND lamp <- ON | RET 1",
		  "parentheses group the names and the message, and ';' separates commands: " + Joined(v, true));

	r.script = "DELAY 1.0 (MSG door1 OPEN)";
	r.frames = Frames(12, 0.1f);
	v = RunPort(r, 1);
	// 0.1f is a little above 0.1, so the tenth frame (9) takes 1.0 to 0 or below and fires it
	const int fDelay = FrameOf(v, "SEND door1 <- OPEN");
	Check(fDelay == 9 && Count(v, "SEND door1 <- OPEN") == 1, "DELAY 1.0 at 0.1 s a frame fires once, in the frame that "
		  "reaches it (frame " + std::to_string(fDelay) + ", the tenth)");

	r.script = "REPEAT 3 3 0.5 0.5 (MSG gun FIRE)";
	r.frames = Frames(40, 0.1f);
	v = RunPort(r, 1);
	Check(Count(v, "SEND gun <- FIRE") == 3, "REPEAT 3 3 sends exactly three times: " + std::to_string(Count(v, "SEND gun <- FIRE")));

	r.script = "LOOPID strobe 0.2 0.2 (MSG light FLASH); DELAYID stop 1.05 (ABORT strobe)";
	r.frames = Frames(30, 0.1f);
	v = RunPort(r, 1);
	const int nFlash = Count(v, "SEND light <- FLASH");
	Check(nFlash >= 4 && nFlash <= 5 && FrameOf(v, "SEND light <- FLASH") >= 0,
		  "LOOPID runs until ABORT with its id stops it: " + std::to_string(nFlash) + " flashes in about a second");

	r.script = "RAND 1.0 (MSG a YES) (MSG a NO)";
	r.frames.clear();
	int nYes = 0;
	for (uint32_t s = 1; s <= 50; ++s) nYes += Count(RunPort(r, s), "SEND a <- YES");
	Check(nYes == 50, "RAND 1.0 always takes the first command (50 of 50)");

	r.script = "RAND2 (MSG a ONE) (MSG a TWO)";
	int nOne = 0, nTwo = 0;
	for (uint32_t s = 1; s <= 200; ++s)
	{
		const std::vector<std::string> w = RunPort(r, s);
		nOne += Count(w, "SEND a <- ONE");
		nTwo += Count(w, "SEND a <- TWO");
	}
	Check(nOne + nTwo == 200 && nOne > 60 && nTwo > 60, "RAND2 picks each command, one at a time (" + std::to_string(nOne) +
		  " and " + std::to_string(nTwo) + " of 200)");

	r.script = "MSG door1";
	v = RunPort(r, 1);
	Check(Count(v, "SEND door1 <- ") == 0 && v.size() > 2 && v[0] == "PRINT CCommandMgr::CheckArgs() ERROR!",
		  "a command with the wrong number of arguments is refused, and says so");

	// The pending limit: 100 delayed commands fit; the 101st runs at once
	std::string many;
	for (int i = 0; i < 101; ++i) many += (i ? "; " : "") + std::string("DELAY 5 (MSG x N") + std::to_string(i) + ")";
	r.script = many;
	r.frames.clear();
	v = RunPort(r, 1);
	Check(Count(v, "SEND x <- N100") == 1 && Count(v, "SEND x <- N99") == 0 && Count(v, "PRINT Pending command buffer overflow!") == 1,
		  "100 commands wait; the 101st finds no slot and runs at once, with NOLF1's warning");

	// Save and load: a manager restored mid script carries on exactly as the one that was saved
	{
		Run a;
		a.script = "REPEATID r 5 5 0.3 0.7 (MSG gun FIRE); LOOP 0.2 0.4 (RAND2 (MSG a X) (MSG a Y)); DELAY 2.0 (MSG door OPEN)";
		a.frames = RaggedFrames(15, 9);
		LT1Mod::CommandMgr* pKept = 0;
		RunPort(a, 3, &pKept);
		const std::string blob = pKept->Save();
		const uint32_t nAt = g_nRand;
		Run b;
		b.frames = RaggedFrames(30, 10);
		g_Log.clear();
		for (size_t f = 0; f < b.frames.size(); ++f) { Log("FRAME " + std::to_string(f)); pKept->Update(b.frames[f]); }
		const std::vector<std::string> cont = g_Log;
		static Host host;
		LT1Mod::CommandMgr restored(host);
		const bool bLoaded = restored.Load(blob);
		g_Log.clear();
		g_nRand = nAt;
		for (size_t f = 0; f < b.frames.size(); ++f) { Log("FRAME " + std::to_string(f)); restored.Update(b.frames[f]); }
		Check(bLoaded && pKept->NumPending() >= 0 && cont == g_Log && Count(cont, "SEND gun <- FIRE") > 0,
			  "a saved manager, loaded, carries on exactly as the original (" + std::to_string(cont.size()) + " lines)");
		Check(!restored.Load(blob.substr(0, blob.size() - 3)) && restored.NumPending() == 0,
			  "a short save loads nothing");
		delete pKept;
	}
}

// 1b. SCRIPT: NOLF1 has no such verb, so the replay never generates it and these answers are
// the port's alone

void TestScriptVerb()
{
	printf("SCRIPT (the port's own verb)\n");
	std::vector<std::string> v;
	Run r;
	r.script = "SCRIPT open_vault 3 (two words); MSG door1 OPEN";
	v = RunPort(r, 1);
	Check(Joined(v, true) == "SCRIPT open_vault|3|two words | SEND door1 <- OPEN | RET 1",
		  "SCRIPT passes its name and every argument, a group as one, and the rest of the string runs: " + Joined(v, true));
	r.script = "SCRIPT nobody; MSG lamp ON";
	v = RunPort(r, 1);
	Check(Count(v, "PRINT Commands: SCRIPT nobody: no script answers it (OnCommand)") == 1 && Count(v, "SEND lamp <- ON") == 1,
		  "a SCRIPT nothing answers says so and is no error: " + Joined(v, true));
	r.script = "DELAY 1.0 (SCRIPT later)";
	r.frames = Frames(12, 0.1f);
	v = RunPort(r, 1);
	Check(Count(v, "SCRIPT later") == 1 && FrameOf(v, "SCRIPT later") >= 8,
		  "a delayed SCRIPT runs once, when its delay is up: " + Joined(v, true));
	r.script = "SCRIPT";
	r.frames.clear();
	v = RunPort(r, 1);
	bool bCalled = false;
	for (size_t i = 0; i < v.size(); ++i) bCalled = bCalled || v[i].compare(0, 7, "SCRIPT ") == 0;
	Check(!bCalled, "SCRIPT with no name is NOLF1's unknown verb, and calls nothing: " + Joined(v, true));
}

// 2. The replay

uint32_t g_nGen = 1;
int Gen(int n) { g_nGen = g_nGen * 1664525u + 1013904223u; return (int)((g_nGen >> 8) % (uint32_t)n); }

std::string GenCmd(int depth);

std::string Sub(int depth) { return "(" + GenCmd(depth + 1) + ")"; }

std::string GenCmd(int depth)
{
	static const char* const kNames[] = { "door1", "lamp", "gun", "(door1 lamp)", "nobody" };
	static const char* const kMsgs[] = { "TRIGGER", "ON", "(LOCK 3)", "OFF", "(PLAY sound.wav)" };
	static const char* const kIds[] = { "a", "b", "strobe", "an_id_longer_than_sixteen", "a" };
	const int nKind = depth >= 3 ? 0 : Gen(16);
	char b[64];
	switch (nKind)
	{
		case 0: case 1: case 2:
			return std::string("MSG ") + kNames[Gen(5)] + " " + kMsgs[Gen(5)];
		case 3:
			snprintf(b, sizeof(b), "DELAY %.2f ", 0.05f * Gen(30));
			return b + Sub(depth);
		case 4:
			snprintf(b, sizeof(b), "DELAYID %s %.2f ", kIds[Gen(5)], 0.05f * Gen(30));
			return b + Sub(depth);
		case 5:
			snprintf(b, sizeof(b), "RAND %.2f ", 0.1f * (1 + Gen(10)));
			return b + Sub(depth) + " " + Sub(depth);
		case 6:
		{
			const int n = 2 + Gen(7);
			std::string s = "RAND" + std::to_string(n);
			for (int i = 0; i < n; ++i) s += " " + Sub(depth);
			return s;
		}
		case 7:
			snprintf(b, sizeof(b), "REPEAT %d %d %.2f %.2f ", Gen(3), 1 + Gen(4), 0.05f * Gen(5), 0.05f + 0.05f * Gen(8));
			return b + Sub(depth);
		case 8:
			snprintf(b, sizeof(b), "REPEATID %s %d %d %.2f %.2f ", kIds[Gen(5)], Gen(3), 1 + Gen(4), 0.05f * Gen(5),
					 0.05f + 0.05f * Gen(8));
			return b + Sub(depth);
		case 9:
			snprintf(b, sizeof(b), "LOOP %.2f %.2f ", 0.05f * Gen(4), 0.1f + 0.1f * Gen(5));
			return b + Sub(depth);
		case 10:
			snprintf(b, sizeof(b), "LOOPID %s %.2f %.2f ", kIds[Gen(5)], 0.05f * Gen(4), 0.1f + 0.1f * Gen(5));
			return b + Sub(depth);
		case 11:
			return std::string("ABORT ") + kIds[Gen(5)];
		case 12:
			return GenCmd(depth + 1) + "; " + GenCmd(depth + 1);
		case 13:
			return "MSG lamp";								// the wrong arity
		case 14:
			return "NOTACOMMAND 1 2";						// ignored, as NOLF1 ignores it
		default:
			return "DELAY -1 (MSG lamp ON)";				// refused: a negative delay
	}
}

void TestReplay()
{
	printf("replay (port against NOLF1's CommandMgr.cpp)\n");
	const char* const kEdges[] =
	{
		"",
		";;;",
		"   MSG    door1    TRIGGER   ",
		"msg door1 trigger",
		"MSG \"door1 lamp\" \"LOCK 3\"",
		"MSG (door1 (lamp)) ((nested) message)",
		"MSG door1 %(escaped%)",
		"LISTCOMMANDS",
		"ABORT nothing_pending",
		"DELAYID abcdefghijklmnopqrstuvwxyz 0.5 (MSG lamp ON); ABORT abcdefghijklmnopqrstuvwxyz",
		"DELAYID abcdefghijklmnopqrstuvwxyz 0.5 (MSG lamp ON); ABORT abcdefghijklmno",
		"REPEAT 0 0 0.1 0.2 (MSG gun FIRE)",
		"REPEAT 2 5 0 0 (MSG gun FIRE)",
		"LOOP 0 0 (MSG lamp ON)",
		"DELAY 0 (DELAY 0 (DELAY 0 (MSG lamp ON)))",
		"RAND 0.0001 (MSG a YES) (MSG a NO)",
		"RAND 1.5 (MSG a YES) (MSG a NO)",
		"RAND9 (MSG a 1) (MSG a 2)",
		"LOOPID x 0.1 0.1 (DELAYID x 0.05 (MSG lamp ON)); DELAY 0.35 (ABORT x)",
		"REPEATID r 3 3 0.1 0.1 (ABORT r)",
		"DELAY 0.1 (MSG a 1); DELAY 0.1 (MSG a 2); DELAY 0.1 (MSG a 3)",
	};
	int nRuns = 0, nBad = 0, nLines = 0;
	std::string first;
	for (size_t i = 0; i < sizeof(kEdges) / sizeof(kEdges[0]); ++i)
	{
		Run r;
		r.script = kEdges[i];
		r.frames = RaggedFrames(40, (uint32_t)(i + 1));
		const std::vector<std::string> a = RunPort(r, (uint32_t)(i + 7)), b = RunNolf(r, (uint32_t)(i + 7));
		const std::string d = FirstDiff(a, b);
		++nRuns; nLines += (int)a.size();
		if (!d.empty()) { if (!nBad) first = "\"" + r.script + "\": " + d; ++nBad; }
	}
	Check(nBad == 0, nBad ? std::to_string(nBad) + " of " + std::to_string(nRuns) + " edge cases differ, first " + first
						  : "the port's log is NOLF1's on all " + std::to_string(nRuns) + " edge cases (" + std::to_string(nLines) +
							" lines: sends, prints, refusals)");

	nRuns = nBad = nLines = 0;
	int nSends = 0;
	g_nGen = 12345;
	for (int i = 0; i < 400; ++i)
	{
		Run r;
		r.script = GenCmd(0);
		for (int k = Gen(3); k > 0; --k) r.script += "; " + GenCmd(0);
		r.frames = RaggedFrames(60 + Gen(60), (uint32_t)(1000 + i));
		const std::vector<std::string> a = RunPort(r, (uint32_t)(77 + i)), b = RunNolf(r, (uint32_t)(77 + i));
		const std::string d = FirstDiff(a, b);
		++nRuns; nLines += (int)a.size();
		for (size_t k = 0; k < a.size(); ++k) nSends += a[k].compare(0, 5, "SEND ") == 0;
		if (!d.empty()) { if (!nBad) first = "\"" + r.script + "\": " + d; ++nBad; }
	}
	Check(nBad == 0, nBad ? std::to_string(nBad) + " of " + std::to_string(nRuns) + " generated scripts differ, first " + first
						  : "the port's log is NOLF1's on all " + std::to_string(nRuns) + " generated scripts (" +
							std::to_string(nLines) + " lines, " + std::to_string(nSends) + " messages sent)");
	Check(nSends > 2000, "the generated scripts exercise the delays and repeats, not only the refusals (" +
		  std::to_string(nSends) + " messages)");
}

// 3. Calibration

void TestCalibration()
{
	printf("calibration\n");
	Run r;
	r.script = "REPEAT 3 3 0.2 0.2 (MSG gun FIRE)";
	r.frames = Frames(30, 0.1f);
	Run bent = r;
	bent.script = "REPEAT 3 4 0.2 0.2 (MSG gun FIRE)";
	bool bCaught = false;
	for (uint32_t s = 1; s < 20 && !bCaught; ++s)
		bCaught = !FirstDiff(RunPort(bent, s), RunNolf(r, s)).empty();
	Check(bCaught, "the comparison catches a one-character change to a script (REPEAT 3 3 against 3 4)");
	bool bSeed = !FirstDiff(RunPort(Run{"RAND2 (MSG a 1) (MSG a 2)", {}}, 1), RunNolf(Run{"RAND2 (MSG a 1) (MSG a 2)", {}}, 2)).empty() ||
				 !FirstDiff(RunPort(Run{"RAND2 (MSG a 1) (MSG a 2)", {}}, 3), RunNolf(Run{"RAND2 (MSG a 1) (MSG a 2)", {}}, 4)).empty();
	Check(bSeed, "and a different random stream under the same script");
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2 || strcmp(argv[1], "--selftest") != 0)
	{
		fprintf(stderr, "usage: testCommands --selftest\n");
		return 2;
	}
	TestKnown();
	TestScriptVerb();
	TestReplay();
	TestCalibration();
	printf("\npassed %d, failed %d\n", g_nPass, g_nFail);
	return g_nFail ? 1 : 0;
}
