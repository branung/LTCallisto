// NOLF1's CommandMgr.cpp with its engine calls behind CommandHost. Each function keeps NOLF1's
// name and order of operations.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lt1mod_command.h"

namespace LT1Mod
{

namespace
{

// The engine's PARSE_MAXARGS and PARSE_MAXARGLEN
const int kParseMaxArgs   = 30;
const int kParseMaxArgLen = 256;

int StrICmp(const char *a, const char *b)
{
    for (;; ++a, ++b)
    {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
}

} // namespace

// What a ConParse holds after one Parse: the arguments of one command
struct CommandMgr::Args
{
    char        buffer[kParseMaxArgs * (kParseMaxArgLen + 1)];
    const char *m_Args[kParseMaxArgs];
    int         m_nArgs;
    Args() : m_nArgs(0) { m_Args[0] = 0; }
};

namespace
{

typedef bool (CommandMgr::*ProcessFn)(CommandMgr::Args &, int);

struct ValidCmd
{
    const char *pCmdName;
    int         nNumArgs;
    ProcessFn   pFn;
    int         nRandArgs;      // RAND2..RAND8: ProcessRandArgs's count, else 0
    const char *pSyntax;
};

// NOLF1's s_ValidCmds, in its order
const ValidCmd s_ValidCmds[] =
{
    { "LISTCOMMANDS", 1, &CommandMgr::ProcessListCommands, 0, "Available Object Commands:" },
    { "MSG",          3, &CommandMgr::ProcessMsg,          0, "  MSG <object name(s)> <object msg>" },
    { "RAND",         4, &CommandMgr::ProcessRand,         0, "  RAND <cmd1 percent> <cmd1> <cmd2>" },
    { "RAND2",        3, 0, 2, "  RAND2 <cmd1> <cmd2>" },
    { "RAND3",        4, 0, 3, "  RAND3 <cmd1> <cmd2> <cmd3>" },
    { "RAND4",        5, 0, 4, "  RAND4 <cmd1> <cmd2> <cmd3> <cmd4>" },
    { "RAND5",        6, 0, 5, "  RAND5 <cmd1> <cmd2> <cmd3> <cmd4> <cmd5>" },
    { "RAND6",        7, 0, 6, "  RAND6 <cmd1> <cmd2> <cmd3> <cmd4> <cmd5> <cmd6>" },
    { "RAND7",        8, 0, 7, "  RAND7 <cmd1> <cmd2> <cmd3> <cmd4> <cmd5> <cmd6> <cmd7>" },
    { "RAND8",        9, 0, 8, "  RAND8 <cmd1> <cmd2> <cmd3> <cmd4> <cmd5> <cmd6> <cmd7> <cmd8>" },
    { "REPEAT",       6, &CommandMgr::ProcessRepeat,       0, "  REPEAT <min times> <max times> <min delay> <max delay> <cmd>" },
    { "REPEATID",     7, &CommandMgr::ProcessRepeatId,     0, "  REPEATID <cmd id> <min times> <max times> <min delay> <max delay> <cmd>" },
    { "DELAY",        3, &CommandMgr::ProcessDelay,        0, "  DELAY <time> <cmd>" },
    { "DELAYID",      4, &CommandMgr::ProcessDelayId,      0, "  DELAYID <cmd id> <time> <cmd>" },
    { "LOOP",         4, &CommandMgr::ProcessLoop,         0, "  LOOP <min delay> <max delay> <cmd>" },
    { "LOOPID",       5, &CommandMgr::ProcessLoopId,       0, "  LOOPID <cmd id> <min delay> <max delay> <cmd>" },
    { "ABORT",        2, &CommandMgr::ProcessAbort,        0, "  ABORT <cmd id>" },
};
const int c_nNumValidCmds = (int)(sizeof(s_ValidCmds) / sizeof(s_ValidCmds[0]));

} // namespace

void CommandMgr::Clear()
{
    for (int i = 0; i < kMaxPending; i++)
        m_Pending[i].Clear();
}

int CommandMgr::NumPending() const
{
    int n = 0;
    for (int i = 0; i < kMaxPending; i++)
        n += m_Pending[i].aCmd[0] != '\0';
    return n;
}

void CommandMgr::Update(float fTimeDelta)
{
    for (int i = 0; i < kMaxPending; i++)
    {
        if (m_Pending[i].aCmd[0] != '\0')
        {
            m_Pending[i].fDelay -= fTimeDelta;

            if (m_Pending[i].fDelay <= 0.0f)
            {
                ProcessCmd(m_Pending[i].aCmd, i);

                // If this is a counted command, decrement the count...
                if (m_Pending[i].nNumTimes >= 0)
                {
                    m_Pending[i].nNumTimes--;
                    if (m_Pending[i].nNumTimes <= 0)
                    {
                        m_Pending[i].Clear();
                        continue;
                    }
                }

                // If this is a repeating command, reset the delay...
                if (m_Pending[i].fMaxDelay > 0.0f)
                {
                    m_Pending[i].fDelay = m_host.RandomFloat(m_Pending[i].fMinDelay, m_Pending[i].fMaxDelay);
                }
                else
                {
                    // Clear the slot
                    m_Pending[i].Clear();
                }
            }
        }
    }
}

bool CommandMgr::ProcessCmd(const char *pCmd, int nCmdIndex)
{
    if (!pCmd || pCmd[0] == '\0') return false;

    // NOLF1's loop over ConParse::Parse: every ';' separated command runs, the last included
    Args parse;
    const char *pPos = pCmd;
    while (pPos)
    {
        const char *pNext = 0;
        const int bMore = m_host.Parse(pPos, &pNext, parse.buffer, parse.m_Args, &parse.m_nArgs);
        pPos = bMore ? pNext : 0;

        if (parse.m_nArgs > 1 && parse.m_Args[0] && StrICmp(parse.m_Args[0], "SCRIPT") == 0)
        {
            if (!ProcessScript(parse, nCmdIndex))
                return false;
        }
        else if (parse.m_nArgs > 0 && parse.m_Args[0])
        {
            for (int i = 0; i < c_nNumValidCmds; i++)
            {
                if (StrICmp(parse.m_Args[0], s_ValidCmds[i].pCmdName) == 0)
                {
                    if (CheckArgs(parse, s_ValidCmds[i].nNumArgs))
                    {
                        const bool bOk = s_ValidCmds[i].nRandArgs ?
                                         ProcessRandArgs(parse, nCmdIndex, s_ValidCmds[i].nRandArgs) :
                                         (this->*s_ValidCmds[i].pFn)(parse, nCmdIndex);
                        if (!bOk)
                            return false;
                    }
                }
            }
        }
    }
    return true;
}

bool CommandMgr::ProcessDelay(Args &parse, int nCmdIndex)
{
    Param cmd;
    cmd.fDelay = (float)atof(parse.m_Args[1]);
    cmd.pCmd   = parse.m_Args[2];
    return AddDelayedCmd(cmd, nCmdIndex);
}

bool CommandMgr::ProcessDelayId(Args &parse, int nCmdIndex)
{
    Param cmd;
    cmd.pId    = parse.m_Args[1];
    cmd.fDelay = (float)atof(parse.m_Args[2]);
    cmd.pCmd   = parse.m_Args[3];
    return AddDelayedCmd(cmd, nCmdIndex);
}

bool CommandMgr::ProcessMsg(Args &parse, int nCmdIndex)
{
    const char *pObjectNames = parse.m_Args[1];
    const char *pMsg         = parse.m_Args[2];

    if (!pObjectNames || !pMsg) return false;

    // NOLF1 parses the names with a second ConParse and sends to every argument of its first command
    Args parse2;
    const char *pNext = 0;
    m_host.Parse(pObjectNames, &pNext, parse2.buffer, parse2.m_Args, &parse2.m_nArgs);
    for (int i = 0; i < parse2.m_nArgs; i++)
    {
        if (m_bTrace)
        {
            char buf[kMaxCommand + 64];
            snprintf(buf, sizeof(buf), "Commands: MSG %s %s", parse2.m_Args[i], pMsg);
            m_host.Print(buf);
        }
        m_host.SendMessage(parse2.m_Args[i], pMsg);
    }

    return true;
}

bool CommandMgr::ProcessRand(Args &parse, int nCmdIndex)
{
    const float fPercent = (float)atof(parse.m_Args[1]);
    const char *pCmd1    = parse.m_Args[2];
    const char *pCmd2    = parse.m_Args[3];

    if (fPercent < 0.001f || fPercent > 1.0f || !pCmd1 || !pCmd2) return false;

    if (m_host.RandomFloat(0.0f, 1.0f) < fPercent)
    {
        return ProcessCmd(pCmd1, nCmdIndex);
    }

    return ProcessCmd(pCmd2, nCmdIndex);
}

bool CommandMgr::ProcessRandArgs(Args &parse, int nCmdIndex, int nNumArgs)
{
    if (nNumArgs < 2) return false;

    const int nCmd = m_host.RandomInt(1, nNumArgs);

    return ProcessCmd(parse.m_Args[nCmd], nCmdIndex);
}

bool CommandMgr::ProcessRepeat(Args &parse, int nCmdIndex)
{
    Param cmd;
    cmd.nMinTimes = (int)atol(parse.m_Args[1]);
    cmd.nMaxTimes = (int)atol(parse.m_Args[2]);
    cmd.fMinDelay = (float)atof(parse.m_Args[3]);
    cmd.fMaxDelay = (float)atof(parse.m_Args[4]);
    cmd.pCmd      = parse.m_Args[5];
    return AddDelayedCmd(cmd, nCmdIndex);
}

bool CommandMgr::ProcessRepeatId(Args &parse, int nCmdIndex)
{
    Param cmd;
    cmd.pId       = parse.m_Args[1];
    cmd.nMinTimes = (int)atol(parse.m_Args[2]);
    cmd.nMaxTimes = (int)atol(parse.m_Args[3]);
    cmd.fMinDelay = (float)atof(parse.m_Args[4]);
    cmd.fMaxDelay = (float)atof(parse.m_Args[5]);
    cmd.pCmd      = parse.m_Args[6];
    return AddDelayedCmd(cmd, nCmdIndex);
}

bool CommandMgr::ProcessLoop(Args &parse, int nCmdIndex)
{
    Param cmd;
    cmd.fMinDelay = (float)atof(parse.m_Args[1]);
    cmd.fMaxDelay = (float)atof(parse.m_Args[2]);
    cmd.pCmd      = parse.m_Args[3];
    return AddDelayedCmd(cmd, nCmdIndex);
}

bool CommandMgr::ProcessLoopId(Args &parse, int nCmdIndex)
{
    Param cmd;
    cmd.pId       = parse.m_Args[1];
    cmd.fMinDelay = (float)atof(parse.m_Args[2]);
    cmd.fMaxDelay = (float)atof(parse.m_Args[3]);
    cmd.pCmd      = parse.m_Args[4];
    return AddDelayedCmd(cmd, nCmdIndex);
}

bool CommandMgr::ProcessAbort(Args &parse, int nCmdIndex)
{
    const char *pId = parse.m_Args[1];
    if (!pId || pId[0] == '\0')
    {
        DevPrint("CCommandMgr::ProcessAbort() ERROR!");
        DevPrint("Invalid Command Id!");
        return false;
    }

    // Remove the specified command...
    for (int i = 0; i < kMaxPending; i++)
    {
        if (m_Pending[i].aCmd[0] != '\0' && m_Pending[i].aId[0] != '\0')
        {
            if (StrICmp(m_Pending[i].aId, pId) == 0)
            {
                m_Pending[i].Clear();
                return true;
            }
        }
    }

    DevPrint("CCommandMgr::ProcessAbort() WARNING!");
    DevPrint("Could not find command associated with id '%s'!", pId);

    return false;
}

bool CommandMgr::AddDelayedCmd(Param &cmd, int nCmdIndex)
{
    if ((cmd.fDelay < 0.0f && cmd.fMaxDelay < 0.0f) || !cmd.pCmd || cmd.pCmd[0] == '\0')
    {
        DevPrint("CCommandMgr::AddDelayedCmd() ERROR!");
        DevPrint("Invalid arguments:");
        DevPrint("  Delay %.2f", cmd.fDelay);
        DevPrint("  MinTimes %d", cmd.nMinTimes);
        DevPrint("  MaxTimes %d", cmd.nMaxTimes);
        DevPrint("  MinDelay %.2f", cmd.fMinDelay);
        DevPrint("  MaxDelay %.2f", cmd.fMaxDelay);
        DevPrint("  Command %s", cmd.pCmd ? cmd.pCmd : "NULL");
        DevPrint("  Id %s", cmd.pId ? cmd.pId : "NULL");
        return false;
    }

    // See if this command was part of a previously pending command...
    bool bWasPending = false;
    if (nCmdIndex >= 0 && nCmdIndex < kMaxPending)
    {
        bWasPending = true;
    }

    // Find a the first open position for the command...
    int i;
    for (i = 0; i < kMaxPending; i++)
    {
        if (m_Pending[i].aCmd[0] == '\0')
        {
            // Make sure we don't try to use the same slot that our parent
            // command was using...
            if (!bWasPending || i != nCmdIndex)
            {
                m_Pending[i].Clear();

                // Set times if appropriate...
                if (cmd.nMaxTimes > 0)
                {
                    m_Pending[i].nMinTimes = cmd.nMinTimes;
                    m_Pending[i].nMaxTimes = cmd.nMaxTimes;
                    m_Pending[i].nNumTimes = m_host.RandomInt(cmd.nMinTimes, cmd.nMaxTimes);
                }

                // Set delay...
                if (cmd.fMaxDelay > 0.0f)
                {
                    m_Pending[i].fMinDelay = cmd.fMinDelay;
                    m_Pending[i].fMaxDelay = cmd.fMaxDelay;
                    m_Pending[i].fDelay    = m_host.RandomFloat(cmd.fMinDelay, cmd.fMaxDelay);
                }
                else
                {
                    m_Pending[i].fDelay = cmd.fDelay;
                }

                // The id isn't used with every command, so it may not be valid...
                if (cmd.pId && cmd.pId[0])
                {
                    strncpy(m_Pending[i].aId, cmd.pId, kMaxId);
                    m_Pending[i].aId[kMaxId - 1] = '\0';
                }

                // Set the command...
                strncpy(m_Pending[i].aCmd, cmd.pCmd, kMaxCommand);
                m_Pending[i].aCmd[kMaxCommand - 1] = '\0';
                break;
            }
        }
    }

    // Make sure we found a slot for it...
    if (i == kMaxPending)
    {
        DevPrint("CCommandMgr::AddDelayedCmd() WARNING!");
        DevPrint("Pending command buffer overflow!");
        DevPrint("Could not delay command '%s'!", cmd.pCmd);
        DevPrint("Processing command now!");
        ProcessCmd(cmd.pCmd, -1);
        return false;
    }

    return true;
}

// SCRIPT <name> [args...]: the script's OnCommand. A name no script answers is reported on the
// console and is no error, so the rest of the string runs.
bool CommandMgr::ProcessScript(Args &parse, int)
{
    if (m_bTrace)
    {
        char buf[kMaxCommand + 64];
        snprintf(buf, sizeof(buf), "Commands: SCRIPT %s (%d argument%s)", parse.m_Args[1], parse.m_nArgs - 2,
                 parse.m_nArgs - 2 == 1 ? "" : "s");
        m_host.Print(buf);
    }
    if (!m_host.RunScript(parse.m_Args[1], parse.m_Args + 2, parse.m_nArgs - 2))
    {
        char buf[kMaxCommand + 64];
        snprintf(buf, sizeof(buf), "Commands: SCRIPT %s: no script answers it (OnCommand)", parse.m_Args[1]);
        m_host.Print(buf);
    }
    return true;
}

bool CommandMgr::IsValidCmd(const char *pCmd)
{
    if (!pCmd || pCmd[0] == '\0') return false;

    Args parse;
    const char *pNext = 0;
    m_host.Parse(pCmd, &pNext, parse.buffer, parse.m_Args, &parse.m_nArgs);
    if (parse.m_nArgs > 1 && parse.m_Args[0] && StrICmp(parse.m_Args[0], "SCRIPT") == 0)
        return true;
    if (parse.m_nArgs > 0 && parse.m_Args[0])
    {
        for (int i = 0; i < c_nNumValidCmds; i++)
        {
            if (StrICmp(parse.m_Args[0], s_ValidCmds[i].pCmdName) == 0)
            {
                return CheckArgs(parse, s_ValidCmds[i].nNumArgs);
            }
        }
    }

    return false;
}

bool CommandMgr::CheckArgs(Args &parse, int nNum)
{
    if (parse.m_nArgs != nNum)
    {
        DevPrint("CCommandMgr::CheckArgs() ERROR!");
        DevPrint("%s command had %d arguments instead of the %d required!", parse.m_Args[0], parse.m_nArgs, nNum);
        for (int i = 0; i < parse.m_nArgs; i++)
        {
            DevPrint("  Arg[%d] = '%s'", i, parse.m_Args[i]);
        }
        return false;
    }

    return true;
}

bool CommandMgr::ProcessListCommands(Args &, int)
{
    for (int i = 0; i < c_nNumValidCmds; i++)
    {
        m_host.Print(s_ValidCmds[i].pSyntax);
    }

    return true;
}

void CommandMgr::DevPrint(const char *msg, ...)
{
    char pMsg[256];
    va_list marker;
    va_start(marker, msg);
    const int nSuccess = vsnprintf(pMsg, sizeof(pMsg), msg, marker);
    va_end(marker);

    if (nSuccess < 0) return;

    m_host.Print(pMsg);
}

// Save and load. NOLF1's record in a byte string: six little endian floats, then each string as
// a 16 bit length and its bytes.

namespace
{

void PutFloat(std::string &out, float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    for (int b = 0; b < 4; ++b) out += (char)((u >> (8 * b)) & 0xff);
}

void PutStr(std::string &out, const char *p)
{
    const size_t n = strlen(p);
    out += (char)(n & 0xff);
    out += (char)((n >> 8) & 0xff);
    out.append(p, n);
}

bool GetFloat(const std::string &in, size_t &at, float &f)
{
    if (at + 4 > in.size()) return false;
    uint32_t u = 0;
    for (int b = 0; b < 4; ++b) u |= (uint32_t)(unsigned char)in[at + b] << (8 * b);
    memcpy(&f, &u, 4);
    at += 4;
    return true;
}

bool GetStr(const std::string &in, size_t &at, char *out, size_t cap)
{
    if (at + 2 > in.size()) return false;
    const size_t n = (size_t)(unsigned char)in[at] | ((size_t)(unsigned char)in[at + 1] << 8);
    at += 2;
    if (at + n > in.size()) return false;
    const size_t k = n < cap - 1 ? n : cap - 1;
    memcpy(out, in.data() + at, k);
    out[k] = '\0';
    at += n;
    return true;
}

} // namespace

std::string CommandMgr::Save() const
{
    std::string out;
    for (int i = 0; i < kMaxPending; i++)
    {
        const Pending &p = m_Pending[i];
        PutFloat(out, p.fDelay);
        PutFloat(out, p.fMinDelay);
        PutFloat(out, p.fMaxDelay);
        PutFloat(out, (float)p.nNumTimes);
        PutFloat(out, (float)p.nMinTimes);
        PutFloat(out, (float)p.nMaxTimes);
        PutStr(out, p.aCmd);
        PutStr(out, p.aId);
    }
    return out;
}

bool CommandMgr::Load(const std::string &blob)
{
    Clear();
    size_t at = 0;
    for (int i = 0; i < kMaxPending; i++)
    {
        Pending &p = m_Pending[i];
        float f[6];
        for (int k = 0; k < 6; ++k)
            if (!GetFloat(blob, at, f[k])) { Clear(); return false; }
        p.fDelay = f[0]; p.fMinDelay = f[1]; p.fMaxDelay = f[2];
        p.nNumTimes = (int)f[3]; p.nMinTimes = (int)f[4]; p.nMaxTimes = (int)f[5];
        if (!GetStr(blob, at, p.aCmd, sizeof(p.aCmd)) || !GetStr(blob, at, p.aId, sizeof(p.aId))) { Clear(); return false; }
    }
    return true;
}

} // namespace LT1Mod
