#ifndef __LT1MOD_COMMAND_H__
#define __LT1MOD_COMMAND_H__

// NOLF1's CommandMgr (NOLF/ObjectDLL/CommandMgr.cpp), ported so the LithTech 1.0 games can run
// its command strings:
//
//     MSG <object name(s)> <message>
//     DELAY <time> <cmd>                    DELAYID <id> <time> <cmd>
//     RAND <cmd1 percent> <cmd1> <cmd2>     RAND2 .. RAND8 <cmd1> .. <cmdN>
//     REPEAT <min times> <max times> <min delay> <max delay> <cmd>
//     REPEATID <id> <min times> <max times> <min delay> <max delay> <cmd>
//     LOOP <min delay> <max delay> <cmd>    LOOPID <id> <min delay> <max delay> <cmd>
//     ABORT <id>                            LISTCOMMANDS
//
// plus one verb NOLF1 never had:
//
//     SCRIPT <name> [args...]               the mod script's OnCommand
//
// SCRIPT takes any number of arguments, so it is matched before NOLF1's table, which wants an
// exact count. Tokenizing is the engine's: spaces separate arguments, parentheses or quotes
// group one, ';' separates commands. The logic is NOLF1's line for line, limits included
// (100 pending commands, 256 character commands, 16 character ids; a delayed command that
// finds no slot runs at once). tests/Commands replays the same scripts through both.
//
// The game is reached only through CommandHost, and only from new entry points (a mod's
// Mod\Commands.txt, a game's Command object), so no retail level changes meaning.

#include <stdint.h>
#include <string>

namespace LT1Mod
{

class CommandHost
{
public:
    virtual ~CommandHost() {}

    // The engine's tokenizer: fills argPointers from argBuffer and nArgs; returns 1 when a ';'
    // ended this command and *pNewCommandPos is where the next starts, 0 when the string is done.
    virtual int Parse(const char *pCommand, const char **pNewCommandPos, char *argBuffer,
                      const char **argPointers, int *nArgs) = 0;
    // Send pMsg to every object named pName, as a level's trigger would
    virtual void SendMessage(const char *pName, const char *pMsg) = 0;
    // The game's GetRandom, both forms
    virtual int   RandomInt(int lo, int hi) = 0;
    virtual float RandomFloat(float lo, float hi) = 0;
    // One line to the console
    virtual void Print(const char *pLine) = 0;
    // SCRIPT <name> [args...]: false when no script answers, which the manager reports
    virtual bool RunScript(const char *pName, const char *const *pArgs, int nArgs)
    {
        (void)pName; (void)pArgs; (void)nArgs;
        return false;
    }
};

class CommandMgr
{
public:
    enum
    {
        kMaxPending  = 100,     // CMDMGR_MAX_PENDING_COMMANDS
        kMaxCommand  = 256,     // CMDMGR_MAX_COMMAND_LENGTH
        kMaxId       = 16       // CMDMGR_MAX_ID_LENGTH
    };

    explicit CommandMgr(CommandHost &host) : m_host(host), m_bTrace(false) {}

    // Run a command string now. False if any command in it was refused.
    bool Process(const char *pCmd) { return ProcessCmd(pCmd, -1); }
    bool IsValidCmd(const char *pCmd);
    // Once a server frame: counts delays down by fFrameTime and runs what is due
    void Update(float fFrameTime);
    // Drops every pending command (a new world)
    void Clear();
    int  NumPending() const;

    // Prints "Commands: MSG <names> <message>" for every message sent. Off by default.
    void SetTrace(bool b) { m_bTrace = b; }

    // The pending commands as NOLF1 saves them (floats for every number, the command and id as
    // strings), in a byte string the game stores in its own save. Load returns false, leaving
    // nothing pending, on a short string.
    std::string Save() const;
    bool Load(const std::string &blob);

    // NOLF1's command processors. Public for the table that names them; call Process.
    struct Args;
    bool ProcessListCommands(Args &a, int nCmdIndex);
    bool ProcessDelay(Args &a, int nCmdIndex);
    bool ProcessDelayId(Args &a, int nCmdIndex);
    bool ProcessMsg(Args &a, int nCmdIndex);
    bool ProcessRand(Args &a, int nCmdIndex);
    bool ProcessRandArgs(Args &a, int nCmdIndex, int nNumArgs);
    bool ProcessRepeat(Args &a, int nCmdIndex);
    bool ProcessRepeatId(Args &a, int nCmdIndex);
    bool ProcessLoop(Args &a, int nCmdIndex);
    bool ProcessLoopId(Args &a, int nCmdIndex);
    bool ProcessAbort(Args &a, int nCmdIndex);
    bool ProcessScript(Args &a, int nCmdIndex);     // not NOLF1's

private:
    struct Pending
    {
        float fDelay, fMinDelay, fMaxDelay;
        int   nNumTimes, nMinTimes, nMaxTimes;
        char  aCmd[kMaxCommand];
        char  aId[kMaxId];
        Pending() { Clear(); }
        void Clear()
        {
            fDelay = fMinDelay = fMaxDelay = 0.0f;
            nNumTimes = nMinTimes = nMaxTimes = -1;
            aCmd[0] = '\0';
            aId[0] = '\0';
        }
    };
    struct Param
    {
        float fDelay, fMinDelay, fMaxDelay;
        int   nNumTimes, nMinTimes, nMaxTimes;
        const char *pCmd;
        const char *pId;
        Param() : fDelay(0.0f), fMinDelay(0.0f), fMaxDelay(0.0f), nNumTimes(-1), nMinTimes(-1), nMaxTimes(-1),
                  pCmd(0), pId(0) {}
    };

    bool ProcessCmd(const char *pCmd, int nCmdIndex);
    bool AddDelayedCmd(Param &cmd, int nCmdIndex);
    bool CheckArgs(Args &a, int nNum);
    void DevPrint(const char *msg, ...);

    CommandHost &m_host;
    bool         m_bTrace;
    Pending      m_Pending[kMaxPending];
};

} // namespace LT1Mod

#endif // __LT1MOD_COMMAND_H__
