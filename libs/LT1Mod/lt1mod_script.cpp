#include "lt1mod_script.h"
#include "lt1mod.h"

#include "lua.hpp"

#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LT1Mod
{

namespace
{

const char* const kServerPath = "Mod\\Script.lua";
const char* const kClientPath = "Mod\\ClientScript.lua";
const char* g_pPath = kServerPath;          // the side's file
ScriptSide g_eSide = kScriptServer;
const char* const kServerEvents[] = {
    "OnWorldStart", "OnCreate", "OnDamage", "OnDeath", "OnCommand",
    "OnWorldEnd", "OnRemove", "OnTrigger", "OnPlayerSpawn", "OnPlayerDeath", "OnPlayerJoin", "OnPlayerLeave",
    "OnUpdate", "OnRestore", "OnClientMessage" };
const char* const kClientEvents[] = { "OnClientStart", "OnWorldStart", "OnDraw", "OnCommand", "OnUpdate", "OnServerMessage" };
const char* const* g_pEvents = kServerEvents;
size_t g_nEvents = sizeof(kServerEvents) / sizeof(kServerEvents[0]);
bool g_bInDraw = false;

// Each message sender's allowance: a bucket of kScriptMessageRate refilled at that rate by the
// game's clock. The client keeps one for itself (key NULL); the server one per player.
struct Allowance { double tokens, last, warned; int dropped; };

// Diagnostics
bool g_bTrace = false;                  // [Options] ScriptTrace
std::set<std::string> g_SlowSaid;       // events said slow this world

// What came in which version on each side (0: not on that side), for the API and the events.
// Script_Lint and the summary's warnings read them; testLT1Mod checks them against the live lists.
struct Versioned { const char* name; int server, client; };
const Versioned kApiNames[] = {
    { "print", 1, 4 }, { "command", 1, 0 }, { "time", 1, 4 }, { "world", 1, 4 }, { "api", 1, 4 },
    { "find", 2, 0 }, { "objects", 2, 0 }, { "players", 2, 0 }, { "after", 2, 4 }, { "every", 2, 4 },
    { "random", 2, 4 }, { "hud", 2, 4 }, { "sound", 2, 4 }, { "set_update_rate", 2, 4 },
    { "require_api", 3, 4 }, { "save", 3, 0 }, { "spawn", 4, 0 }, { "client", 5, 0 }, { "server", 0, 5 },
    { "explode", 6, 0 },
    { "hud.text", 0, 4 }, { "hud.rect", 0, 4 } };
const Versioned kEventNames[] = {
    { "OnWorldStart", 1, 4 }, { "OnCreate", 1, 0 }, { "OnDamage", 1, 0 }, { "OnDeath", 1, 0 }, { "OnCommand", 1, 4 },
    { "OnWorldEnd", 2, 0 }, { "OnRemove", 2, 0 }, { "OnTrigger", 2, 0 }, { "OnPlayerSpawn", 2, 0 },
    { "OnPlayerDeath", 2, 0 }, { "OnPlayerJoin", 2, 0 }, { "OnPlayerLeave", 2, 0 }, { "OnUpdate", 2, 4 },
    { "OnRestore", 3, 0 }, { "OnClientMessage", 5, 0 },
    { "OnClientStart", 0, 4 }, { "OnDraw", 0, 4 }, { "OnServerMessage", 0, 5 } };

const Versioned* FindVersioned(const Versioned* p, size_t n, const std::string& name)
{
    for (size_t i = 0; i < n; ++i)
        if (name == p[i].name) return &p[i];
    return NULL;
}

std::map<void*, Allowance> g_Allowance;
int g_nDropped = 0;
const char* const kObjectMeta = "LT1Mod.Object";
const char* const kTimerMeta  = "LT1Mod.Timer";

lua_State*              g_L = NULL;
ScriptHost*             g_pHost = NULL;
size_t                  g_nMemory = 0;
long                    g_nBudget = 0;
int                     g_nDepth = 0;           // calls in progress (a command can raise another event)
std::set<std::string>   g_Disabled;             // handlers an error turned off, until the next world

// The mod's live objects. Each has one userdata, kept in the Lua registry's object table under
// its handle, so the same object is the same Lua value in every event; the userdata's serial
// matches the entry's while the object lives.
struct Entry
{
    unsigned    serial;
    std::string name, archetype, cls;
    bool        player;
    bool        removing;       // OnRemove is running: only the strings are left
};
std::map<void*, Entry>  g_Objects;
unsigned                g_nSerial = 0;
int                     g_nObjTable = LUA_NOREF;    // registry ref: handle -> userdata

struct Obj { void* h; unsigned serial; };

// Timers (mod.after, mod.every), by the game's clock.
struct Timer
{
    int     id;
    double  due, period;        // period 0: once
    int     fnRef;              // the function, in the registry
    bool    dead;
};
std::vector<Timer>      g_Timers;
int                     g_nTimerId = 0;

bool                    g_bWorld = false;       // between a WorldStart and its WorldEnd
std::string             g_sText;                // the chunk, for the rebuild at each world's end
// Modules (API 7): their text by name, read while the script loads and kept for each world's
// rebuild; the order they loaded in; and the mod's files, which only a load can read
std::map<std::string, std::string> g_ModuleText;
std::vector<std::string> g_ModuleOrder;
FileSource*             g_pSrc = NULL;
std::string             g_sCarried;             // mod.save, serialized, across a rebuild
std::string             g_sRestore;             // a save's blob, for the next WorldStart
bool                    g_bRestore = false;
double                  g_fUpdatePeriod = 0.0;  // 0: no OnUpdate
double                  g_fNextUpdate = 0.0;
double                  g_fLastUpdate = -1.0;

// Lua's allocator, capped. For a new block Lua passes a type tag in osize, not a size.
void* Alloc(void*, void* ptr, size_t osize, size_t nsize)
{
    const size_t old = ptr ? osize : 0;
    if (nsize == 0)
    {
        g_nMemory -= old;
        free(ptr);
        return NULL;
    }
    if (nsize > old && g_nMemory + (nsize - old) > kScriptMemory)
        return NULL;
    void* p = realloc(ptr, nsize);
    if (p) g_nMemory = g_nMemory - old + nsize;
    return p;
}

void Hook(lua_State* L, lua_Debug*)
{
    g_nBudget -= 1000;
    if (g_nBudget <= 0)
        luaL_error(L, "the script ran past its budget of instructions for this call");
}

int Traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg ? msg : "(an error that is not a string)", 1);
    return 1;
}

void PrintLines(const std::string& msg)
{
    if (!g_pHost) return;
    for (size_t b = 0, e; b < msg.size(); b = e + 1)
    {
        e = msg.find('\n', b);
        if (e == std::string::npos) e = msg.size();
        g_pHost->Print(msg.substr(b, e - b).c_str());
    }
}

// Runs the function at the stack's top with nArgs above it, under the budget,
// and reports an error as the game's console would show it.
Entry* Find(void* h);

// A trace line's arguments: "\"boss\", 7.5, DemoTrooper 'trooper1'". Formatted with no
// metamethod, since a raise would longjmp past the strings held here (see ModSpawn).
std::string TraceArgs(lua_State* L, int first, int last)
{
    std::string s;
    for (int i = first; i <= last; ++i)
    {
        if (i > first) s += ", ";
        char b[96];
        switch (lua_type(L, i))
        {
            case LUA_TNIL:      s += "nil"; break;
            case LUA_TBOOLEAN:  s += lua_toboolean(L, i) ? "true" : "false"; break;
            case LUA_TNUMBER:
                if (lua_isinteger(L, i)) snprintf(b, sizeof(b), "%lld", (long long)lua_tointeger(L, i));
                else snprintf(b, sizeof(b), "%.14g", (double)lua_tonumber(L, i));
                s += b;
                break;
            case LUA_TSTRING:
            {
                size_t n = 0;
                const char* p = lua_tolstring(L, i, &n);    // a string already: no conversion
                s += '"';
                s.append(p, n < 60 ? n : 60);
                if (n > 60) s += "...";
                s += '"';
                break;
            }
            default:
            {
                Obj* o = (Obj*)luaL_testudata(L, i, kObjectMeta);
                Entry* e = o ? Find(o->h) : NULL;
                if (o && e && e->serial == o->serial)
                    s += (e->player ? std::string("player") : e->archetype.empty() ? e->cls : e->archetype) + " '" + e->name + "'";
                else if (o)
                    s += "(a gone object)";
                else
                {
                    snprintf(b, sizeof(b), "%s: %p", luaL_typename(L, i), lua_topointer(L, i));
                    s += b;
                }
                break;
            }
        }
    }
    return s;
}

// An API function under ScriptTrace: says the call, then makes it. An error from it is raised
// again with the caller's line and the function's name, which the wrapper would otherwise hide.
int TraceCall(lua_State* L)
{
    const int n = lua_gettop(L);
    if (g_pHost)
        g_pHost->Print((std::string(g_pPath) + ": trace: " + lua_tostring(L, lua_upvalueindex(1)) + "(" +
                        TraceArgs(L, 1, n) + ")").c_str());
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 1);
    if (lua_pcall(L, n, LUA_MULTRET, 0) != LUA_OK)
    {
        if (lua_type(L, -1) == LUA_TSTRING)
        {
            // Rebuilt on the Lua stack, and the C++ string gone before lua_error (see ModSpawn)
            {
                std::string msg = lua_tostring(L, -1);
                const size_t q = msg.find(" to '?'");
                if (q != std::string::npos) msg.replace(q, 7, std::string(" to '") + lua_tostring(L, lua_upvalueindex(1)) + "'");
                lua_pop(L, 1);
                lua_pushstring(L, msg.c_str());
            }
            luaL_where(L, 1);
            lua_insert(L, -2);
            lua_concat(L, 2);
        }
        return lua_error(L);
    }
    return lua_gettop(L);
}

// Every C function in the table at idx, wrapped by TraceCall as prefix + name.
void TraceTable(lua_State* L, int idx, const char* pPrefix)
{
    idx = lua_absindex(L, idx);
    std::vector<std::string> keys;
    lua_pushnil(L);
    while (lua_next(L, idx))
    {
        if (lua_type(L, -2) == LUA_TSTRING && lua_iscfunction(L, -1)) keys.push_back(lua_tostring(L, -2));
        lua_pop(L, 1);
    }
    for (size_t i = 0; i < keys.size(); ++i)
    {
        lua_pushstring(L, (std::string(pPrefix) + keys[i]).c_str());
        lua_getfield(L, idx, keys[i].c_str());
        lua_pushcclosure(L, TraceCall, 2);
        lua_setfield(L, idx, keys[i].c_str());
    }
}

bool Call(int nArgs, const char* pWhat, std::string* pError, long nBudget = kScriptBudget)
{
    if (g_nDepth == 0)
    {
        g_nBudget = nBudget;
        lua_sethook(g_L, Hook, LUA_MASKCOUNT, 1000);
    }
    const int base = lua_gettop(g_L) - nArgs;
    if (g_bTrace && g_pHost)
        g_pHost->Print((std::string(g_pPath) + ": trace: " + pWhat + "(" + TraceArgs(g_L, base + 1, base + nArgs) + ")").c_str());
    lua_pushcfunction(g_L, Traceback);
    lua_insert(g_L, base);
    const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ++g_nDepth;
    const int rc = lua_pcall(g_L, nArgs, 0, base);
    --g_nDepth;
    lua_remove(g_L, base);
    if (g_nDepth == 0)
    {
        lua_sethook(g_L, NULL, 0, 0);
        // A slow event names itself, once a world; every timer is one name.
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        const std::string key = strncmp(pWhat, "timer ", 6) == 0 ? "a timer" : pWhat;
        if (ms > kScriptSlowMs && g_pHost && g_SlowSaid.insert(key).second)
        {
            char b[200];
            snprintf(b, sizeof(b), "%s: %s took %.1f ms, more than %.0f, inside the game's frame (said once a world)",
                     g_pPath, key.c_str(), ms, kScriptSlowMs);
            g_pHost->Print(b);
        }
    }
    if (rc == LUA_OK)
        return true;
    const std::string msg = std::string(g_pPath) + ": " + pWhat + ": " +
                            (lua_tostring(g_L, -1) ? lua_tostring(g_L, -1) : "an error");
    lua_pop(g_L, 1);
    if (pError) *pError = msg;
    else PrintLines(msg);
    return false;
}

void Disable(const char* pEvent)
{
    g_Disabled.insert(pEvent);
    if (g_pHost) g_pHost->Print((std::string(g_pPath) + ": " + pEvent + " is off until the next world").c_str());
}

// Pushes the handler named pEvent; false when the script has none, or an
// error disabled it.
bool PushHandler(const char* pEvent)
{
    if (!g_L || g_Disabled.count(pEvent)) return false;
    lua_getglobal(g_L, pEvent);
    if (lua_isfunction(g_L, -1)) return true;
    lua_pop(g_L, 1);
    return false;
}

// Objects

Entry* Find(void* h)
{
    std::map<void*, Entry>::iterator it = g_Objects.find(h);
    return it == g_Objects.end() ? NULL : &it->second;
}

// Registers an object, if it is not yet, and pushes its userdata.
void PushObject(void* h, const char* pName, const char* pArch, const char* pClass, bool bPlayer)
{
    Entry* e = Find(h);
    if (!e)
    {
        Entry n;
        n.serial = ++g_nSerial;
        n.name = pName ? pName : "";
        n.archetype = pArch ? pArch : "";
        n.cls = pClass ? pClass : "";
        n.player = bPlayer;
        n.removing = false;
        e = &(g_Objects[h] = n);
    }
    else if (e->player && pName && pName[0])
        e->name = pName;    // a player's name is the game's latest: Blood 2's arrives after the player
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_nObjTable);
    lua_rawgetp(g_L, -1, h);
    Obj* o = lua_isuserdata(g_L, -1) ? (Obj*)lua_touserdata(g_L, -1) : NULL;
    if (!o || o->serial != e->serial)
    {
        lua_pop(g_L, 1);
        o = (Obj*)lua_newuserdatauv(g_L, sizeof(Obj), 0);
        o->h = h;
        o->serial = e->serial;
        luaL_setmetatable(g_L, kObjectMeta);
        lua_pushvalue(g_L, -1);
        lua_rawsetp(g_L, -3, h);
    }
    lua_remove(g_L, -2);
}

void PushObject(const ScriptObject& so, bool bPlayer)
{
    PushObject(so.hObj, so.pName, so.pArchetype, so.pClass, bPlayer);
}

// The entry an object userdata at index i names, or NULL when it is gone.
Entry* Live(lua_State* L, int i)
{
    Obj* o = (Obj*)luaL_checkudata(L, i, kObjectMeta);
    Entry* e = Find(o->h);
    return (e && e->serial == o->serial) ? e : NULL;
}

// For a method that reaches the game: the live handle, or a script error.
void* CheckObj(lua_State* L)
{
    Entry* e = Live(L, 1);
    if (!e) luaL_error(L, "the object is gone");
    if (e->removing) luaL_error(L, "the object is being removed: only its name, archetype and class are left");
    return ((Obj*)lua_touserdata(L, 1))->h;
}
// For a method that reads the strings, which removal leaves.
Entry* CheckEntry(lua_State* L)
{
    Entry* e = Live(L, 1);
    if (!e) luaL_error(L, "the object is gone");
    return e;
}

int ObjValid(lua_State* L)     { Entry* e = Live(L, 1); lua_pushboolean(L, e && !e->removing); return 1; }
int ObjName(lua_State* L)      { lua_pushstring(L, CheckEntry(L)->name.c_str()); return 1; }
int ObjArchetype(lua_State* L) { lua_pushstring(L, CheckEntry(L)->archetype.c_str()); return 1; }
int ObjClass(lua_State* L)     { lua_pushstring(L, CheckEntry(L)->cls.c_str()); return 1; }
int ObjIsPlayer(lua_State* L)  { lua_pushboolean(L, CheckEntry(L)->player); return 1; }

int ObjHealth(lua_State* L)
{
    void* h = CheckObj(L);
    float f = 0.0f;
    if (!g_pHost || !g_pHost->GetHealth(h, f)) return luaL_error(L, "the object has no health");
    lua_pushnumber(L, f);
    return 1;
}
int ObjMaxHealth(lua_State* L)
{
    void* h = CheckObj(L);
    float f = 0.0f;
    if (!g_pHost || !g_pHost->GetMaxHealth(h, f)) return luaL_error(L, "the object has no maximum health");
    lua_pushnumber(L, f);
    return 1;
}
int ObjSetHealth(lua_State* L)
{
    void* h = CheckObj(L);
    const lua_Number n = luaL_checknumber(L, 2);
    if (n < 0) return luaL_error(L, "set_health wants 0 or more");
    if (!g_pHost || !g_pHost->SetHealth(h, (float)n)) return luaL_error(L, "the object has no health");
    return 0;
}
int ObjDamage(lua_State* L)
{
    void* h = CheckObj(L);
    const lua_Number n = luaL_checknumber(L, 2);
    if (n <= 0) return luaL_error(L, "damage wants more than 0");
    if (!g_pHost || !g_pHost->Damage(h, (float)n)) return luaL_error(L, "the object cannot be damaged");
    return 0;
}
int ObjPos(lua_State* L)
{
    void* h = CheckObj(L);
    float x = 0, y = 0, z = 0;
    if (!g_pHost || !g_pHost->GetPos(h, x, y, z)) return luaL_error(L, "the object has no position");
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int ObjTeleport(lua_State* L)
{
    void* h = CheckObj(L);
    const float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3), z = (float)luaL_checknumber(L, 4);
    if (!g_pHost || !g_pHost->Teleport(h, x, y, z)) return luaL_error(L, "the object cannot be moved");
    return 0;
}
int ObjSend(lua_State* L)
{
    void* h = CheckObj(L);
    const char* msg = luaL_checkstring(L, 2);
    if (!g_pHost || !g_pHost->SendTrigger(h, msg)) return luaL_error(L, "the object takes no messages");
    return 0;
}
int ObjRemove(lua_State* L)
{
    void* h = CheckObj(L);
    if (CheckEntry(L)->player) return luaL_error(L, "a player cannot be removed");
    if (!g_pHost || !g_pHost->Remove(h)) return luaL_error(L, "the object cannot be removed");
    return 0;
}
int ObjToString(lua_State* L)
{
    Entry* e = Live(L, 1);
    if (!e) { lua_pushstring(L, "(a gone object)"); return 1; }
    lua_pushfstring(L, "%s '%s'", e->player ? "player" : (e->archetype.empty() ? e->cls : e->archetype).c_str(), e->name.c_str());
    return 1;
}

// mod.*

// mod.print / print: each argument tostring'd, tab-joined, one console line.
int ModPrint(lua_State* L)
{
    // A Lua buffer, not a std::string: a value's __tostring can raise and longjmp past this frame
    const int n = lua_gettop(L);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    // In single player both scripts print to one console, so each says whose.
    luaL_addstring(&b, g_eSide == kScriptClient ? "ClientScript: " : "Script: ");
    for (int i = 1; i <= n; ++i)
    {
        if (i > 1) luaL_addchar(&b, '\t');
        luaL_tolstring(L, i, NULL);
        luaL_addvalue(&b);
    }
    luaL_pushresult(&b);
    if (g_pHost) g_pHost->Print(lua_tostring(L, -1));
    return 0;
}

int ModCommand(lua_State* L)
{
    const char* cmd = luaL_checkstring(L, 1);
    if (g_pHost) g_pHost->Command(cmd);
    return 0;
}

int ModTime(lua_State* L)
{
    lua_pushnumber(L, g_pHost ? g_pHost->Time() : 0.0);
    return 1;
}

int ModFind(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    for (std::map<void*, Entry>::iterator it = g_Objects.begin(); it != g_Objects.end(); ++it)
        if (!it->second.player && !it->second.removing && it->second.name == name)
        {
            PushObject(it->first, NULL, NULL, NULL, false);
            return 1;
        }
    lua_pushnil(L);
    return 1;
}

int ListObjects(lua_State* L, bool bPlayers, const char* pArch)
{
    lua_newtable(L);
    int n = 0;
    // Sorted by serial, the order the objects came into the game, so a list is
    // the same in every run.
    std::vector<std::pair<unsigned, void*> > order;
    for (std::map<void*, Entry>::iterator it = g_Objects.begin(); it != g_Objects.end(); ++it)
        if (it->second.player == bPlayers && !it->second.removing && (!pArch || it->second.archetype == pArch))
            order.push_back(std::make_pair(it->second.serial, it->first));
    std::sort(order.begin(), order.end());
    for (size_t i = 0; i < order.size(); ++i)
    {
        PushObject(order[i].second, NULL, NULL, NULL, bPlayers);
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}
int ModObjects(lua_State* L) { return ListObjects(L, false, lua_isnoneornil(L, 1) ? NULL : luaL_checkstring(L, 1)); }
int ModPlayers(lua_State* L) { return ListObjects(L, true, NULL); }

int AddTimer(lua_State* L, bool bRepeat)
{
    const lua_Number s = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (s < 0 || (bRepeat && s < 0.01)) return luaL_error(L, bRepeat ? "every wants 0.01 seconds or more" : "after wants 0 or more seconds");
    if (g_Timers.size() >= 256) return luaL_error(L, "too many timers (256)");
    Timer t;
    t.id = ++g_nTimerId;
    t.due = (g_pHost ? g_pHost->Time() : 0.0) + s;
    t.period = bRepeat ? s : 0.0;
    lua_pushvalue(L, 2);
    t.fnRef = luaL_ref(L, LUA_REGISTRYINDEX);
    t.dead = false;
    g_Timers.push_back(t);
    int* p = (int*)lua_newuserdatauv(L, sizeof(int), 0);
    *p = t.id;
    luaL_setmetatable(L, kTimerMeta);
    return 1;
}
int ModAfter(lua_State* L) { return AddTimer(L, false); }
int ModEvery(lua_State* L) { return AddTimer(L, true); }
int TimerCancel(lua_State* L)
{
    const int id = *(int*)luaL_checkudata(L, 1, kTimerMeta);
    for (size_t i = 0; i < g_Timers.size(); ++i)
        if (g_Timers[i].id == id) g_Timers[i].dead = true;
    return 0;
}

int ModRandom(lua_State* L)
{
    if (lua_isnoneornil(L, 1))
    {
        const int r = g_pHost ? g_pHost->RandomInt(0, 1 << 24) : 0;
        lua_pushnumber(L, (lua_Number)(r % (1 << 24)) / (lua_Number)(1 << 24));
        return 1;
    }
    const lua_Integer lo = luaL_checkinteger(L, 1), hi = luaL_checkinteger(L, 2);
    if (hi < lo) return luaL_error(L, "random wants lo <= hi");
    lua_pushinteger(L, g_pHost ? g_pHost->RandomInt((int)lo, (int)hi) : lo);
    return 1;
}

int ModHud(lua_State* L)
{
    void* h = NULL;
    if (!lua_isnil(L, 1))
    {
        Entry* e = Live(L, 1);
        if (!e || !e->player) return luaL_error(L, "hud wants a player, or nil for every player");
        h = ((Obj*)lua_touserdata(L, 1))->h;
    }
    const char* text = luaL_checkstring(L, 2);
    if (!g_pHost || !g_pHost->Hud(h, text)) return luaL_error(L, "this game shows no hud line");
    return 0;
}

// Messages

// Why a message cannot be one, or NULL.
const char* Malformed(const char* pName, size_t nName, size_t nPayload, bool bNul)
{
    if (!pName || nName == 0) return "a message wants a name";
    if (nName > kScriptMessageName) return "a message's name is longer than 64 bytes";
    if (nPayload > kScriptMessageMax) return "a message's payload is longer than 1024 bytes";
    if (bNul || strlen(pName) != nName) return "a message is text: it has a NUL byte";
    return NULL;
}

// Whether sender `key` may send one now; counts and says (once a second) the
// ones it may not.
bool Admit(void* key, const char* pWho)
{
    const double now = g_pHost ? g_pHost->Time() : 0.0;
    std::map<void*, Allowance>::iterator it = g_Allowance.find(key);
    if (it == g_Allowance.end())
    {
        Allowance a = { (double)kScriptMessageRate, now, -1.0, 0 };
        it = g_Allowance.insert(std::make_pair(key, a)).first;
    }
    Allowance& a = it->second;
    a.tokens += (now - a.last) * kScriptMessageRate;
    if (a.tokens > kScriptMessageRate) a.tokens = kScriptMessageRate;
    a.last = now;
    if (a.tokens >= 1.0) { a.tokens -= 1.0; return true; }
    ++a.dropped;
    ++g_nDropped;
    if (g_pHost && (a.warned < 0.0 || now - a.warned >= 1.0))
    {
        a.warned = now;
        char b[200];
        snprintf(b, sizeof(b), "%s: %s sent more than %d messages a second: %d dropped so far",
                 g_pPath, pWho, kScriptMessageRate, a.dropped);
        g_pHost->Print(b);
    }
    return false;
}

void Dropped(const char* pWhy)
{
    ++g_nDropped;
    if (g_pHost) g_pHost->Print((std::string(g_pPath) + ": a message was dropped: " + pWhy).c_str());
}

// mod.client(player | nil, name [, payload])
int ModClient(lua_State* L)
{
    void* h = NULL;
    if (!lua_isnil(L, 1))
    {
        Entry* e = Live(L, 1);
        if (!e || !e->player) return luaL_error(L, "mod.client wants a player, or nil for every player");
        h = ((Obj*)lua_touserdata(L, 1))->h;
    }
    size_t nName = 0, nPayload = 0;
    const char* pName = luaL_checklstring(L, 2, &nName);
    const char* pPayload = luaL_optlstring(L, 3, "", &nPayload);
    if (const char* why = Malformed(pName, nName, nPayload, strlen(pPayload) != nPayload))
        return luaL_error(L, "mod.client: %s", why);
    lua_pushboolean(L, g_pHost && g_pHost->SendToClient(h, pName, pPayload));
    return 1;
}

int ModSound(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3), z = (float)luaL_checknumber(L, 4);
    if (!g_pHost || !g_pHost->Sound(path, x, y, z)) return luaL_error(L, "the sound did not play");
    return 0;
}

int ModSpawn(lua_State* L)
{
    const char* arch = luaL_checkstring(L, 1);
    const float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3), z = (float)luaL_checknumber(L, 4);
    const char* props = luaL_optstring(L, 5, "");
    const float yaw = (float)luaL_optnumber(L, 6, 0.0);
    if (!g_pHost) return luaL_error(L, "this game spawns nothing");
    // The reason is copied out and its string gone before luaL_error, which
    // longjmps past this frame: Lua is built as C, so a live std::string here
    // would never be destroyed (and on MSVC, whether it is is unspecified).
    void* h = NULL;
    bool bObject = false;
    char why[200] = "";
    {
        std::string sWhy;
        h = g_pHost->SpawnEx(arch, x, y, z, props, yaw, bObject, sWhy);
        snprintf(why, sizeof(why), "%s", sWhy.c_str());
    }
    if (!h) return luaL_error(L, "spawn %s: %s", arch, why);
    // Made, and OnCreate has normally registered it already; else now, if it is an object the
    // script may hold (version 6: a retail class the mod has not opted in is made, and answered
    // true).
    Entry* e = Find(h);
    if (!e && !bObject)
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    PushObject(h, e ? NULL : "", e ? NULL : arch, e ? NULL : arch, false);
    return 1;
}

// Version 6: mod.explode(x, y, z, radius, min, max [, effect [, source]]).
int ModExplode(lua_State* L)
{
    const float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2), z = (float)luaL_checknumber(L, 3);
    const float r = (float)luaL_checknumber(L, 4), lo = (float)luaL_checknumber(L, 5), hi = (float)luaL_checknumber(L, 6);
    const char* effect = luaL_optstring(L, 7, "");
    void* hSource = NULL;
    if (!lua_isnoneornil(L, 8))
    {
        Entry* e = Live(L, 8);
        if (!e || e->removing) return luaL_error(L, "explode: the source object is gone");
        hSource = ((Obj*)lua_touserdata(L, 8))->h;
    }
    if (!(r > 0.0f) || !(lo >= 0.0f) || !(hi >= lo)) return luaL_error(L, "explode wants a radius above 0 and 0 <= min <= max");
    if (!g_pHost) return luaL_error(L, "this game has no explosions for a script");
    bool ok = false;
    char why[200] = "";
    {
        std::string sWhy;
        ok = g_pHost->Explode(x, y, z, r, lo, hi, effect, hSource, sWhy);
        snprintf(why, sizeof(why), "%s", sWhy.c_str());
    }
    if (!ok) return luaL_error(L, "explode: %s", why);
    return 0;
}

int ModRequireApi(lua_State* L)
{
    const lua_Integer n = luaL_checkinteger(L, 1);
    if (n > kScriptApi)
        return luaL_error(L, "this script needs mod.api %d; this game has %d", (int)n, kScriptApi);
    return 0;
}

int ModSetUpdateRate(lua_State* L)
{
    const lua_Number hz = luaL_checknumber(L, 1);
    if (hz < 0 || hz > kScriptMaxUpdateRate) return luaL_error(L, "set_update_rate wants 0 to %d", kScriptMaxUpdateRate);
    g_fUpdatePeriod = hz > 0 ? 1.0 / hz : 0.0;
    g_fNextUpdate = (g_pHost ? g_pHost->Time() : 0.0) + g_fUpdatePeriod;
    return 0;
}

// The client's
ScriptClientHost* ClientHost() { return g_eSide == kScriptClient ? static_cast<ScriptClientHost*>(g_pHost) : NULL; }

int HudText(lua_State* L)
{
    if (!g_bInDraw) return luaL_error(L, "mod.hud.text draws only inside OnDraw");
    const int x = (int)luaL_checknumber(L, 1), y = (int)luaL_checknumber(L, 2);
    const char* text = luaL_checkstring(L, 3);
    const int r = (int)luaL_optinteger(L, 4, 255), g = (int)luaL_optinteger(L, 5, 255), b = (int)luaL_optinteger(L, 6, 255);
    ScriptClientHost* h = ClientHost();
    if (!h || !h->DrawHudText(x, y, text, r, g, b)) return luaL_error(L, "the text was not drawn");
    return 0;
}

int HudRect(lua_State* L)
{
    if (!g_bInDraw) return luaL_error(L, "mod.hud.rect draws only inside OnDraw");
    const int x = (int)luaL_checknumber(L, 1), y = (int)luaL_checknumber(L, 2);
    const int w = (int)luaL_checknumber(L, 3), hh = (int)luaL_checknumber(L, 4);
    const int r = (int)luaL_checkinteger(L, 5), g = (int)luaL_checkinteger(L, 6), b = (int)luaL_checkinteger(L, 7);
    if (w < 0 || hh < 0) return luaL_error(L, "mod.hud.rect wants a size of 0 or more");
    ScriptClientHost* h = ClientHost();
    if (!h || !h->DrawRect(x, y, w, hh, r, g, b)) return luaL_error(L, "the rectangle was not drawn");
    return 0;
}

int ClientSound(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    ScriptClientHost* h = ClientHost();
    if (!h || !h->Sound2D(path)) return luaL_error(L, "the sound did not play");
    return 0;
}

// mod.server(name [, payload]), the client's
int ModServer(lua_State* L)
{
    size_t nName = 0, nPayload = 0;
    const char* pName = luaL_checklstring(L, 1, &nName);
    const char* pPayload = luaL_optlstring(L, 2, "", &nPayload);
    if (const char* why = Malformed(pName, nName, nPayload, strlen(pPayload) != nPayload))
        return luaL_error(L, "mod.server: %s", why);
    ScriptClientHost* h = ClientHost();
    if (!Admit(NULL, "this client")) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, h && h->SendToServer(pName, pPayload));
    return 1;
}

// require (MODULES, lt1mod_script.h): Mod\Scripts\<a>\<b>.lua, once. The registry keeps
// "lt1mod.loaded", name -> result, with false while one loads.
int ModRequire(lua_State* L)
{
    const char* pName = luaL_checkstring(L, 1);
    const std::string name = pName;
    bool bOk = !name.empty() && name.size() <= kScriptModuleName && name[0] != '.' && name[name.size() - 1] != '.';
    for (size_t i = 0; bOk && i < name.size(); ++i)
    {
        const char c = name[i];
        bOk = isalnum((unsigned char)c) || c == '_' || (c == '.' && name[i + 1] != '.');
    }
    if (!bOk)
        return luaL_error(L, "require(\"%s\"): a module's name is letters, digits and _ joined by dots "
                             "(\"ec.revenants\" is Mod\\Scripts\\ec\\revenants.lua), at most %d characters",
                          pName, (int)kScriptModuleName);
    lua_getfield(L, LUA_REGISTRYINDEX, "lt1mod.loaded");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "lt1mod.loaded");
    }
    const int loaded = lua_gettop(L);
    lua_getfield(L, loaded, pName);
    if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
        return luaL_error(L, "require(\"%s\"): a cycle -- it is still loading, and requires itself through "
                             "what it requires", pName);
    if (!lua_isnil(L, -1)) return 1;		// loaded: its result
    lua_pop(L, 1);

    std::string path = "Mod\\Scripts\\";
    for (size_t i = 0; i < name.size(); ++i) path += name[i] == '.' ? '\\' : name[i];
    path += ".lua";
    std::map<std::string, std::string>::const_iterator it = g_ModuleText.find(name);
    if (it == g_ModuleText.end())
    {
        if (!g_pSrc)
            return luaL_error(L, "require(\"%s\"): modules are read while the script loads; require it at the "
                                 "top of a file", pName);
        if (g_ModuleText.size() >= kScriptModules)
            return luaL_error(L, "require(\"%s\"): more than %d modules", pName, (int)kScriptModules);
        std::string text;
        if (!g_pSrc->ReadFile(path.c_str(), text))
            return luaL_error(L, "require(\"%s\"): no %s in the mod", pName, path.c_str());
        it = g_ModuleText.insert(std::make_pair(name, text)).first;
        g_ModuleOrder.push_back(name);
    }
    if (luaL_loadbufferx(L, it->second.data(), it->second.size(), ("@" + path).c_str(), "t") != LUA_OK)
        return lua_error(L);
    lua_pushboolean(L, 0);
    lua_setfield(L, loaded, pName);		// loading
    lua_pushstring(L, pName);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK)
    {
        lua_pushnil(L);
        lua_setfield(L, loaded, pName);	// not loading any more: an error is not a cycle
        return lua_error(L);
    }
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushboolean(L, 1); }
    lua_pushvalue(L, -1);
    lua_setfield(L, loaded, pName);
    return 1;
}

void OpenSandbox(lua_State* L)
{
    const luaL_Reg libs[] = {
        { LUA_GNAME, luaopen_base }, { LUA_COLIBNAME, luaopen_coroutine }, { LUA_TABLIBNAME, luaopen_table },
        { LUA_STRLIBNAME, luaopen_string }, { LUA_MATHLIBNAME, luaopen_math }, { LUA_UTF8LIBNAME, luaopen_utf8 },
    };
    for (size_t i = 0; i < sizeof(libs) / sizeof(libs[0]); ++i)
    {
        luaL_requiref(L, libs[i].name, libs[i].func, 1);
        lua_pop(L, 1);
    }
    // No file and no code from outside the chunk.
    const char* const removed[] = { "dofile", "loadfile", "load", "require" };
    for (size_t i = 0; i < sizeof(removed) / sizeof(removed[0]); ++i)
    {
        lua_pushnil(L);
        lua_setglobal(L, removed[i]);
    }
    // ...but the mod's own modules, by name (API 7, MODULES)
    lua_pushcfunction(L, ModRequire);
    lua_setglobal(L, "require");

    const luaL_Reg serverFns[] = {
        { "print", ModPrint }, { "command", ModCommand }, { "time", ModTime },
        { "find", ModFind }, { "objects", ModObjects }, { "players", ModPlayers },
        { "after", ModAfter }, { "every", ModEvery }, { "random", ModRandom },
        { "hud", ModHud }, { "sound", ModSound }, { "set_update_rate", ModSetUpdateRate },
        { "require_api", ModRequireApi }, { "spawn", ModSpawn }, { "client", ModClient }, { "explode", ModExplode },
        { NULL, NULL } };
    const luaL_Reg clientFns[] = {
        { "print", ModPrint }, { "time", ModTime },
        { "after", ModAfter }, { "every", ModEvery }, { "random", ModRandom },
        { "sound", ClientSound }, { "set_update_rate", ModSetUpdateRate },
        { "require_api", ModRequireApi }, { "server", ModServer }, { NULL, NULL } };
    if (g_eSide == kScriptClient)
    {
        luaL_newlib(L, clientFns);
        const luaL_Reg hudFns[] = { { "text", HudText }, { "rect", HudRect }, { NULL, NULL } };
        luaL_newlib(L, hudFns);
        if (g_bTrace) TraceTable(L, -1, "mod.hud.");
        lua_setfield(L, -2, "hud");
    }
    else
    {
        luaL_newlib(L, serverFns);
        lua_newtable(L);
        lua_setfield(L, -2, "save");
    }
    if (g_bTrace) TraceTable(L, -1, "mod.");
    lua_pushinteger(L, kScriptApi);
    lua_setfield(L, -2, "api");
    lua_pushstring(L, "");
    lua_setfield(L, -2, "world");
    lua_setglobal(L, "mod");
    lua_pushcfunction(L, ModPrint);
    lua_setglobal(L, "print");
    // The game's stream, not Lua's own; randomseed would reseed nothing, so it is gone
    lua_getglobal(L, "math");
    lua_pushcfunction(L, ModRandom);
    lua_setfield(L, -2, "random");
    lua_pushnil(L);
    lua_setfield(L, -2, "randomseed");
    lua_pop(L, 1);

    const luaL_Reg objFns[] = {
        { "valid", ObjValid }, { "name", ObjName }, { "archetype", ObjArchetype }, { "class", ObjClass },
        { "is_player", ObjIsPlayer }, { "health", ObjHealth }, { "max_health", ObjMaxHealth },
        { "set_health", ObjSetHealth }, { "damage", ObjDamage }, { "pos", ObjPos }, { "teleport", ObjTeleport },
        { "send", ObjSend }, { "remove", ObjRemove }, { NULL, NULL } };
    luaL_newmetatable(L, kObjectMeta);
    luaL_newlib(L, objFns);
    if (g_bTrace) TraceTable(L, -1, "obj:");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ObjToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushstring(L, kObjectMeta);
    lua_setfield(L, -2, "__metatable");     // getmetatable answers this, setmetatable refuses
    lua_pop(L, 1);

    const luaL_Reg timerFns[] = { { "cancel", TimerCancel }, { NULL, NULL } };
    luaL_newmetatable(L, kTimerMeta);
    luaL_newlib(L, timerFns);
    if (g_bTrace) TraceTable(L, -1, "timer:");
    lua_setfield(L, -2, "__index");
    lua_pushstring(L, kTimerMeta);
    lua_setfield(L, -2, "__metatable");
    lua_pop(L, 1);

    lua_newtable(L);
    g_nObjTable = luaL_ref(L, LUA_REGISTRYINDEX);
}

// Raises an object event: pushes the object, then pAmount or the two strings.
void Raise(const char* pEvent, const ScriptObject& so, bool bPlayer, const float* pAmount = NULL,
           const char* pStr1 = NULL, const char* pStr2 = NULL)
{
    if (!g_L) return;
    if (!PushHandler(pEvent))
    {
        // Registered even unheard, so mod.find and mod.objects know it.
        PushObject(so, bPlayer);
        lua_pop(g_L, 1);
        return;
    }
    PushObject(so, bPlayer);
    int nArgs = 1;
    if (pAmount) { lua_pushnumber(g_L, *pAmount); ++nArgs; }
    if (pStr1)   { lua_pushstring(g_L, pStr1); ++nArgs; }
    if (pStr2)   { lua_pushstring(g_L, pStr2); ++nArgs; }
    if (!Call(nArgs, pEvent, NULL)) Disable(pEvent);
}

void ForgetWorld()
{
    if (g_L)
    {
        for (size_t i = 0; i < g_Timers.size(); ++i) luaL_unref(g_L, LUA_REGISTRYINDEX, g_Timers[i].fnRef);
        lua_newtable(g_L);
        lua_rawseti(g_L, LUA_REGISTRYINDEX, g_nObjTable);
    }
    g_Timers.clear();
    g_Objects.clear();
    g_fUpdatePeriod = 0.0;
    g_fLastUpdate = -1.0;
}

// mod.save

void PutU32(std::string& out, uint32_t v) { for (int i = 0; i < 4; ++i) out += (char)((v >> (8 * i)) & 0xff); }
void PutStr(std::string& out, const std::string& s) { PutU32(out, (uint32_t)s.size()); out += s; }
void PutNum(std::string& out, double d) { char b[8]; memcpy(b, &d, 8); out.append(b, 8); }
struct Reader
{
    const std::string& s; size_t p; bool bad;
    explicit Reader(const std::string& str) : s(str), p(0), bad(false) {}
    bool Need(size_t n) { if (p + n > s.size()) bad = true; return !bad; }
    uint32_t U32() { if (!Need(4)) return 0; uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= (uint32_t)(unsigned char)s[p + i] << (8 * i); p += 4; return v; }
    std::string Str() { const uint32_t n = U32(); if (!Need(n)) return ""; std::string r = s.substr(p, n); p += n; return r; }
    double Num() { if (!Need(8)) return 0.0; double d; memcpy(&d, s.data() + p, 8); p += 8; return d; }
    char Tag() { if (!Need(1)) return 0; return s[p++]; }
};

// The value at index i, or false with sWhy for one that cannot be saved.
bool Serialize(lua_State* L, int i, std::string& out, int depth, std::string& sWhy)
{
    i = lua_absindex(L, i);
    if (depth > 16) { sWhy = "tables nested more than 16 deep"; return false; }
    switch (lua_type(L, i))
    {
        case LUA_TNIL:     out += 'n'; return true;
        case LUA_TBOOLEAN: out += lua_toboolean(L, i) ? 'T' : 'F'; return true;
        case LUA_TNUMBER:
            // Lua 5.4 keeps integers apart from floats, and so does a save.
            if (lua_isinteger(L, i))
            {
                const long long v = (long long)lua_tointeger(L, i);
                char b[8]; memcpy(b, &v, 8);
                out += 'i'; out.append(b, 8);
            }
            else { out += 'd'; PutNum(out, lua_tonumber(L, i)); }
            return true;
        case LUA_TSTRING:
        {
            size_t n; const char* p = lua_tolstring(L, i, &n);
            out += 's'; PutStr(out, std::string(p, n)); return true;
        }
        case LUA_TTABLE:
        {
            std::string body;
            uint32_t n = 0;
            lua_pushnil(L);
            while (lua_next(L, i))
            {
                if (!Serialize(L, -2, body, depth + 1, sWhy) || !Serialize(L, -1, body, depth + 1, sWhy))
                {
                    lua_pop(L, 2);
                    return false;
                }
                ++n;
                lua_pop(L, 1);
            }
            out += 't'; PutU32(out, n); out += body;
            return true;
        }
        case LUA_TUSERDATA:
        {
            Obj* o = (Obj*)luaL_testudata(L, i, kObjectMeta);
            Entry* e = o ? Find(o->h) : NULL;
            if (o && e && e->serial == o->serial && !e->player)
            {
                out += 'o'; PutStr(out, e->name); PutStr(out, e->archetype); return true;
            }
            if (o) { out += 'n'; return true; }     // a gone object, or a player: nil
            break;
        }
        default: break;
    }
    sWhy = std::string("a ") + lua_typename(L, lua_type(L, i)) + " cannot be saved";
    return false;
}

// Pushes the value a Serialize wrote; an object is found again by its name
// and archetype among the live ones, or is nil.
void Deserialize(lua_State* L, Reader& r, int depth)
{
    if (depth > 16) { r.bad = true; lua_pushnil(L); return; }
    switch (r.Tag())
    {
        case 'n': lua_pushnil(L); return;
        case 'T': lua_pushboolean(L, 1); return;
        case 'F': lua_pushboolean(L, 0); return;
        case 'd': lua_pushnumber(L, r.Num()); return;
        case 'i':
        {
            long long v = 0;
            if (r.Need(8)) { memcpy(&v, r.s.data() + r.p, 8); r.p += 8; }
            lua_pushinteger(L, (lua_Integer)v);
            return;
        }
        case 's': { const std::string v = r.Str(); lua_pushlstring(L, v.data(), v.size()); return; }
        case 't':
        {
            const uint32_t n = r.U32();
            lua_newtable(L);
            for (uint32_t k = 0; k < n && !r.bad; ++k)
            {
                Deserialize(L, r, depth + 1);
                Deserialize(L, r, depth + 1);
                if (lua_isnil(L, -2)) lua_pop(L, 2);
                else lua_rawset(L, -3);
            }
            return;
        }
        case 'o':
        {
            const std::string name = r.Str(), arch = r.Str();
            for (std::map<void*, Entry>::iterator it = g_Objects.begin(); it != g_Objects.end(); ++it)
                if (!it->second.player && !it->second.removing && it->second.name == name && it->second.archetype == arch)
                {
                    PushObject(it->first, NULL, NULL, NULL, false);
                    return;
                }
            lua_pushnil(L);
            return;
        }
        default: r.bad = true; lua_pushnil(L); return;
    }
}

// mod.save, serialized; "" when it cannot be (said once, as an error).
std::string SaveTable()
{
    if (!g_L || g_eSide == kScriptClient) return "";
    lua_getglobal(g_L, "mod");
    lua_getfield(g_L, -1, "save");
    std::string out, why;
    const bool bOk = lua_istable(g_L, -1) && Serialize(g_L, -1, out, 0, why);
    lua_pop(g_L, 2);
    if (!bOk || out.size() > kScriptSaveMax)
    {
        if (bOk) why = "it is larger than 64 KB";
        else if (why.empty()) why = "it is not a table";
        if (g_pHost) g_pHost->Print((std::string(g_pPath) + ": mod.save is not saved: " + why).c_str());
        return "";
    }
    return out;
}

void LoadTable(const std::string& data)
{
    if (!g_L || data.empty() || g_eSide == kScriptClient) return;
    Reader r(data);
    lua_getglobal(g_L, "mod");
    Deserialize(g_L, r, 0);
    if (r.bad || !lua_istable(g_L, -1))
    {
        lua_pop(g_L, 1);
        if (g_pHost) g_pHost->Print((std::string(g_pPath) + ": the saved mod.save is damaged; it starts empty").c_str());
        lua_newtable(g_L);
    }
    lua_setfield(g_L, -2, "save");
    lua_pop(g_L, 1);
}

// The global a function value is, or "".
std::string GlobalNameOf(int fnRef)
{
    std::string name;
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, fnRef);
    lua_pushglobaltable(g_L);
    lua_pushnil(g_L);
    while (lua_next(g_L, -2))
    {
        if (lua_type(g_L, -2) == LUA_TSTRING && lua_rawequal(g_L, -1, -4))
            name = lua_tostring(g_L, -2);
        lua_pop(g_L, 1);
    }
    lua_pop(g_L, 2);
    return name;
}

// A fresh state for the chunk: the next world's
bool Rebuild()
{
    ScriptHost* pHost = g_pHost;
    const std::string text = g_sText;
    if (g_L) lua_close(g_L);
    g_L = NULL;
    g_nMemory = 0;
    g_nDepth = 0;
    g_Disabled.clear();
    g_Timers.clear();
    g_Objects.clear();
    g_fUpdatePeriod = 0.0;
    g_fLastUpdate = -1.0;
    lua_State* L = lua_newstate(Alloc, NULL);
    if (!L) return false;
    g_L = L;
    g_pHost = pHost;
    OpenSandbox(L);
    std::string err;
    if (luaL_loadbufferx(L, text.data(), text.size(), (std::string("@") + g_pPath).c_str(), "t") != LUA_OK)
        err = std::string(g_pPath) + ": " + lua_tostring(L, -1);
    else
        Call(0, "the top level", &err);
    if (!err.empty())
    {
        PrintLines(err + "\n" + g_pPath + ": the script is off for the rest of the game");
        lua_close(g_L);
        g_L = NULL;
        return false;
    }
    return true;
}

} // namespace

void Script_Clear()
{
    if (g_L) lua_close(g_L);
    g_L = NULL;
    g_Timers.clear();
    g_Objects.clear();
    g_pHost = NULL;
    g_nMemory = 0;
    g_nDepth = 0;
    g_Disabled.clear();
    g_fUpdatePeriod = 0.0;
    g_fLastUpdate = -1.0;
    g_nObjTable = LUA_NOREF;
    g_bWorld = false;
    g_sText.clear();
    g_ModuleText.clear();
    g_ModuleOrder.clear();
    g_pSrc = NULL;
    g_sCarried.clear();
    g_sRestore.clear();
    g_bRestore = false;
    g_Allowance.clear();
    g_nDropped = 0;
    g_SlowSaid.clear();
    g_bTrace = false;
}

bool Script_Loaded() { return g_L != NULL; }
bool Script_Present() { return !g_sText.empty(); }

bool Script_Load(FileSource& src, ScriptHost* pHost, std::string& sError, ScriptSide side)
{
    Script_Clear();
    sError.clear();
    g_eSide = side;
    g_pPath = side == kScriptClient ? kClientPath : kServerPath;
    g_pEvents = side == kScriptClient ? kClientEvents : kServerEvents;
    g_nEvents = side == kScriptClient ? sizeof(kClientEvents) / sizeof(kClientEvents[0])
                                      : sizeof(kServerEvents) / sizeof(kServerEvents[0]);
    g_bTrace = Active().valid && Active().manifest.scriptTrace;
    if (!Active().present || !Active().valid)
        return true;
    std::string text;
    if (!src.ReadFile(g_pPath, text))
        return true;        // no script: nothing to run

    lua_State* L = lua_newstate(Alloc, NULL);
    if (!L) { sError = std::string(g_pPath) + ": no memory for the script"; return false; }
    g_L = L;
    g_pHost = pHost;
    OpenSandbox(L);
    // Text only: a precompiled chunk is refused, since its bytecode is
    // unchecked.
    if (luaL_loadbufferx(L, text.data(), text.size(), (std::string("@") + g_pPath).c_str(), "t") != LUA_OK)
    {
        sError = std::string(g_pPath) + ": " + lua_tostring(L, -1);
        Script_Clear();
        return false;
    }
    g_pSrc = &src;		// modules are read while the top level runs (MODULES)
    const bool bTop = Call(0, "the top level", &sError);
    g_pSrc = NULL;
    if (!bTop)
    {
        Script_Clear();
        return false;
    }
    g_sText = text;
    return true;
}

std::string Script_Summary()
{
    if (!g_L) return "";
    std::string handlers, warnings;
    for (size_t i = 0; i < g_nEvents; ++i)
    {
        lua_getglobal(g_L, g_pEvents[i]);
        if (lua_isfunction(g_L, -1)) handlers += std::string(handlers.empty() ? "" : ", ") + g_pEvents[i];
        lua_pop(g_L, 1);
    }
    lua_pushglobaltable(g_L);
    lua_pushnil(g_L);
    std::vector<std::string> odd;
    while (lua_next(g_L, -2))
    {
        if (lua_type(g_L, -2) == LUA_TSTRING && lua_isfunction(g_L, -1))
        {
            const std::string k = lua_tostring(g_L, -2);
            bool bEvent = false;
            for (size_t i = 0; i < g_nEvents; ++i)
                if (k == g_pEvents[i]) bEvent = true;
            if (!bEvent && k.size() > 2 && k[0] == 'O' && k[1] == 'n' && isupper((unsigned char)k[2]))
                odd.push_back(k);
        }
        lua_pop(g_L, 1);
    }
    lua_pop(g_L, 1);
    std::sort(odd.begin(), odd.end());
    const char* const pOther = g_eSide == kScriptClient ? kServerPath : kClientPath;
    for (size_t i = 0; i < odd.size(); ++i)
    {
        const Versioned* v = FindVersioned(kEventNames, sizeof(kEventNames) / sizeof(kEventNames[0]), odd[i]);
        if (v && (g_eSide == kScriptClient ? v->server : v->client))
            warnings += "\nWARNING: " + odd[i] + " is an event of " + pOther + ", not of this file, so nothing here calls it";
        else
            warnings += "\nWARNING: " + odd[i] + " is not an event, so nothing calls it";
    }
    std::string modules;
    for (size_t i = 0; i < g_ModuleOrder.size(); ++i)
        modules += std::string(i ? ", " : "; modules ") + g_ModuleOrder[i];
    return (handlers.empty() ? std::string("no event handlers") : "handles " + handlers) + modules + warnings;
}

void Script_WorldStart(const char* pWorld, bool bRestoring)
{
    if (!g_L) return;
    g_Disabled.clear();
    g_SlowSaid.clear();
    g_bWorld = true;
    lua_getglobal(g_L, "mod");
    lua_pushstring(g_L, pWorld ? pWorld : "");
    lua_setfield(g_L, -2, "world");
    lua_pop(g_L, 1);
    const char* pEvent = "OnWorldStart";
    if (bRestoring)
    {
        pEvent = "OnRestore";
        if (g_bRestore)
        {
            // The save's mod.save, its references found among the objects
            // the restore brought back (Script_Adopt), then its timers.
            Reader r(g_sRestore);
            const bool bMagic = g_sRestore.compare(0, 4, "LTS3") == 0;
            r.p = 4;
            const std::string table = bMagic ? r.Str() : std::string();
            LoadTable(table);
            const uint32_t nTimers = bMagic ? r.U32() : 0;
            const double now = g_pHost ? g_pHost->Time() : 0.0;
            for (uint32_t t = 0; t < nTimers && !r.bad; ++t)
            {
                const std::string name = r.Str();
                const double left = r.Num(), period = r.Num();
                lua_getglobal(g_L, name.c_str());
                if (!lua_isfunction(g_L, -1))
                {
                    lua_pop(g_L, 1);
                    if (g_pHost) g_pHost->Print((std::string(g_pPath) + ": a saved timer's function " + name +
                                                 " is gone; the timer is not restored").c_str());
                    continue;
                }
                Timer tm;
                tm.id = ++g_nTimerId;
                tm.due = now + left;
                tm.period = period;
                tm.fnRef = luaL_ref(g_L, LUA_REGISTRYINDEX);
                tm.dead = false;
                g_Timers.push_back(tm);
            }
            if (r.bad && g_pHost) g_pHost->Print((std::string(g_pPath) + ": the save's script data is damaged").c_str());
            g_bRestore = false;
            g_sRestore.clear();
        }
    }
    if (!PushHandler(pEvent)) return;
    lua_pushstring(g_L, pWorld ? pWorld : "");
    if (!Call(1, pEvent, NULL)) Disable(pEvent);
}

void Script_WorldEnd()
{
    // Before the first world there is none to end: a game's PreStartWorld
    // runs before every world, the first one too.
    if (!g_L || !g_bWorld) return;
    g_bWorld = false;
    if (PushHandler("OnWorldEnd"))
    {
        lua_getglobal(g_L, "mod");
        lua_getfield(g_L, -1, "world");
        lua_remove(g_L, -2);
        if (!Call(1, "OnWorldEnd", NULL)) Disable("OnWorldEnd");
    }
    // What lasts, then a fresh state for the next world
    g_sCarried = SaveTable();
    ForgetWorld();
    if (Rebuild())
        LoadTable(g_sCarried);
}

std::string Script_SaveBlob()
{
    if (!g_L) return "";
    std::string out = "LTS3";
    PutStr(out, SaveTable());
    const double now = g_pHost ? g_pHost->Time() : 0.0;
    std::string timers;
    uint32_t n = 0;
    for (size_t i = 0; i < g_Timers.size(); ++i)
    {
        if (g_Timers[i].dead) continue;
        const std::string name = GlobalNameOf(g_Timers[i].fnRef);
        if (name.empty())
        {
            if (g_pHost)
            {
                char b[160];
                snprintf(b, sizeof(b), "%s: timer %d's function is not a global, so it is not saved", g_pPath, g_Timers[i].id);
                g_pHost->Print(b);
            }
            continue;
        }
        PutStr(timers, name);
        PutNum(timers, g_Timers[i].due > now ? g_Timers[i].due - now : 0.0);
        PutNum(timers, g_Timers[i].period);
        ++n;
    }
    PutU32(out, n);
    out += timers;
    return out;
}

void Script_RestoreBlob(const std::string& blob)
{
    g_sRestore = blob;
    g_bRestore = !blob.empty();
}

void Script_Adopt(const ScriptObject& o)
{
    if (!g_L) return;
    PushObject(o, false);
    lua_pop(g_L, 1);
}

void Script_Update()
{
    if (!g_L || !g_pHost) return;
    const double now = g_pHost->Time();
    // Timers due, in the order they fall due; a timer a timer sets waits for
    // the next frame.
    std::vector<int> due;
    for (size_t i = 0; i < g_Timers.size(); ++i)
        if (!g_Timers[i].dead && g_Timers[i].due <= now) due.push_back(g_Timers[i].id);
    for (size_t d = 0; d < due.size() && g_L; ++d)
    {
        size_t i = 0;
        while (i < g_Timers.size() && g_Timers[i].id != due[d]) ++i;
        if (i == g_Timers.size() || g_Timers[i].dead) continue;
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_Timers[i].fnRef);
        if (g_Timers[i].period > 0.0)
            g_Timers[i].due += g_Timers[i].period * floor((now - g_Timers[i].due) / g_Timers[i].period + 1.0);
        else
            g_Timers[i].dead = true;
        char what[32];
        snprintf(what, sizeof(what), "timer %d", due[d]);
        if (!Call(0, what, NULL))
        {
            for (size_t j = 0; j < g_Timers.size(); ++j)
                if (g_Timers[j].id == due[d]) g_Timers[j].dead = true;
            if (g_pHost) g_pHost->Print((std::string(g_pPath) + ": " + what + " is off").c_str());
        }
    }
    for (size_t i = 0; i < g_Timers.size();)
        if (g_Timers[i].dead) { if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, g_Timers[i].fnRef); g_Timers.erase(g_Timers.begin() + (long)i); }
        else ++i;

    if (g_fUpdatePeriod > 0.0 && now >= g_fNextUpdate && PushHandler("OnUpdate"))
    {
        const double dt = g_fLastUpdate < 0.0 ? g_fUpdatePeriod : now - g_fLastUpdate;
        g_fLastUpdate = now;
        g_fNextUpdate = now + g_fUpdatePeriod;
        lua_pushnumber(g_L, dt);
        if (!Call(1, "OnUpdate", NULL)) Disable("OnUpdate");
    }
}

// Version 6's arguments: an object the script already knows, else nil (an attacker it has never
// seen is no object to it), and a string or nil.
namespace
{
void PushKnown(void* h)
{
    Entry* e = h ? Find(h) : NULL;
    if (e && !e->removing) PushObject(h, NULL, NULL, NULL, e->player);
    else lua_pushnil(g_L);
}
void PushOptString(const char* p)
{
    if (p) lua_pushstring(g_L, p);
    else lua_pushnil(g_L);
}
// The object is pushed (and so registered) whether or not the event is heard.
bool BeginEvent(const char* pEvent, const ScriptObject& so)
{
    if (!g_L) return false;
    if (!PushHandler(pEvent))
    {
        PushObject(so, false);
        lua_pop(g_L, 1);
        return false;
    }
    PushObject(so, false);
    return true;
}
} // namespace

void Script_Create(const ScriptObject& o, bool bPlaced)
{
    if (!BeginEvent("OnCreate", o)) return;
    lua_pushboolean(g_L, bPlaced ? 1 : 0);
    if (!Call(2, "OnCreate", NULL)) Disable("OnCreate");
}
void Script_Damage(const ScriptObject& o, float fAmount, void* hFrom, const char* pKind)
{
    if (!BeginEvent("OnDamage", o)) return;
    lua_pushnumber(g_L, fAmount);
    PushKnown(hFrom);
    PushOptString(pKind);
    if (!Call(4, "OnDamage", NULL)) Disable("OnDamage");
}
void Script_Death(const ScriptObject& o, void* hKiller, const char* pKind, float fDamage)
{
    if (!BeginEvent("OnDeath", o)) return;
    PushKnown(hKiller);
    PushOptString(pKind);
    if (fDamage >= 0.0f) lua_pushnumber(g_L, fDamage);
    else lua_pushnil(g_L);
    if (!Call(4, "OnDeath", NULL)) Disable("OnDeath");
}
void Script_Trigger(const ScriptObject& o, const char* pMsg, const char* pFrom)
{
    Raise("OnTrigger", o, false, NULL, pMsg ? pMsg : "", pFrom ? pFrom : "");
}
void Script_PlayerSpawn(const ScriptObject& o)           { Raise("OnPlayerSpawn", o, true); }
void Script_PlayerDeath(const ScriptObject& o)           { Raise("OnPlayerDeath", o, true); }
void Script_PlayerJoin(const ScriptObject& o)            { Raise("OnPlayerJoin", o, true); }
void Script_PlayerLeave(const ScriptObject& o)
{
    Raise("OnPlayerLeave", o, true);
    Script_Removed(o.hObj);
}

void Script_Removed(void* hObj)
{
    Entry* e = Find(hObj);
    if (!g_L || !e) { g_Objects.erase(hObj); return; }
    if (!e->player && PushHandler("OnRemove"))
    {
        e->removing = true;
        PushObject(hObj, NULL, NULL, NULL, false);
        if (!Call(1, "OnRemove", NULL)) Disable("OnRemove");
    }
    g_Objects.erase(hObj);
    if (g_L)
    {
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_nObjTable);
        lua_pushnil(g_L);
        lua_rawsetp(g_L, -2, hObj);
        lua_pop(g_L, 1);
    }
}

bool Script_Function(const char* pFunction, const char* const* pArgs, int nArgs)
{
    if (!g_L || !pFunction || !pFunction[0] || g_Disabled.count(pFunction)) return false;
    lua_getglobal(g_L, pFunction);
    if (!lua_isfunction(g_L, -1)) { lua_pop(g_L, 1); return false; }
    for (int i = 0; i < nArgs; ++i) lua_pushstring(g_L, pArgs[i] ? pArgs[i] : "");
    if (!Call(nArgs, pFunction, NULL)) Disable(pFunction);
    return true;
}

bool Script_Command(const char* pName, const char* const* pArgs, int nArgs)
{
    if (!PushHandler("OnCommand")) return false;
    lua_pushstring(g_L, pName ? pName : "");
    for (int i = 0; i < nArgs; ++i) lua_pushstring(g_L, pArgs[i] ? pArgs[i] : "");
    if (!Call(1 + nArgs, "OnCommand", NULL)) Disable("OnCommand");
    return true;
}

namespace
{

// The static pass, for Script_Lint

struct Tok { std::string s; int line; char kind; };     // kind: n name, u number, s string, p punctuation

// The level of a long bracket at i ("[[" 0, "[==[" 2), or -1.
int LongLevel(const std::string& t, size_t i)
{
    if (i >= t.size() || t[i] != '[') return -1;
    size_t j = i + 1;
    while (j < t.size() && t[j] == '=') ++j;
    return (j < t.size() && t[j] == '[') ? (int)(j - i - 1) : -1;
}

// From a long bracket's opening to past its close, counting lines.
void SkipLong(const std::string& t, size_t& i, int level, int& line)
{
    const std::string close = "]" + std::string(level, '=') + "]";
    const size_t end = t.find(close, i);
    const size_t stop = end == std::string::npos ? t.size() : end + close.size();
    for (; i < stop; ++i) if (t[i] == '\n') ++line;
}

// Lua's tokens, as far as the pass needs them: comments and strings skipped
// (a mod.x in either is no call), names, numbers and punctuation.
std::vector<Tok> Tokenize(const std::string& t)
{
    std::vector<Tok> out;
    int line = 1;
    size_t i = 0;
    while (i < t.size())
    {
        const char c = t[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (isspace((unsigned char)c)) { ++i; continue; }
        if (c == '-' && i + 1 < t.size() && t[i + 1] == '-')
        {
            i += 2;
            const int level = LongLevel(t, i);
            if (level >= 0) SkipLong(t, i, level, line);
            else while (i < t.size() && t[i] != '\n') ++i;
            continue;
        }
        const int level = LongLevel(t, i);
        if (level >= 0)
        {
            Tok k = { "", line, 's' };
            SkipLong(t, i, level, line);
            out.push_back(k);
            continue;
        }
        if (c == '"' || c == '\'')
        {
            Tok k = { "", line, 's' };
            for (++i; i < t.size() && t[i] != c; ++i)
            {
                if (t[i] == '\\' && i + 1 < t.size()) { if (t[i + 1] == '\n') ++line; ++i; }
                else if (t[i] == '\n') { ++line; break; }   // unfinished: the compiler says so
            }
            ++i;
            out.push_back(k);
            continue;
        }
        const size_t b = i;
        char kind = 'p';
        if (isalpha((unsigned char)c) || c == '_')
        {
            kind = 'n';
            while (i < t.size() && (isalnum((unsigned char)t[i]) || t[i] == '_')) ++i;
        }
        else if (isdigit((unsigned char)c))
        {
            kind = 'u';
            while (i < t.size() && (isalnum((unsigned char)t[i]) || t[i] == '.' || t[i] == '_')) ++i;
        }
        else if (t.compare(i, 3, "...") == 0) i += 3;
        else if (t.compare(i, 2, "..") == 0 || t.compare(i, 2, "::") == 0 || t.compare(i, 2, "==") == 0) i += 2;
        else ++i;
        Tok k = { t.substr(b, i - b), line, kind };
        out.push_back(k);
    }
    return out;
}

} // namespace

std::vector<std::string> Script_Modules() { return g_ModuleOrder; }

std::string Script_Lint(const std::string& text, ScriptSide side)
{
    return Script_LintModule(text, side, NULL);
}

std::string Script_LintModule(const std::string& text, ScriptSide side, const char* pModulePath)
{
    const bool bClient = side == kScriptClient;
    const char* const pPath = pModulePath ? pModulePath : bClient ? kClientPath : kServerPath;
    const char* const pOther = bClient ? kServerPath : kClientPath;
    const std::vector<Tok> k = Tokenize(text);
    const size_t nk = k.size();
    struct At
    {
        const std::vector<Tok>& k;
        bool Is(size_t j, const char* s) const { return j < k.size() && k[j].kind != 's' && k[j].s == s; }
        bool Name(size_t j) const { return j < k.size() && k[j].kind == 'n'; }
    } at = { k };
    struct Use { std::string what; int line, version; };
    std::vector<Use> uses;
    std::vector<std::string> lines;
    std::set<std::string> said;
    int target = 0;
    for (size_t j = 0; j < nk; ++j)
    {
        std::string warning;
        // mod.<name> and mod.hud.<name>, where mod is not itself a field.
        if (at.Name(j) && k[j].s == "mod" && !(j > 0 && (at.Is(j - 1, ".") || at.Is(j - 1, ":"))) &&
            at.Is(j + 1, ".") && at.Name(j + 2))
        {
            std::string name = k[j + 2].s;
            if (name == "hud" && at.Is(j + 3, ".") && at.Name(j + 4)) name += "." + k[j + 4].s;
            if (name == "require_api" && at.Is(j + 3, "(") && j + 4 < nk && k[j + 4].kind == 'u')
                target = atoi(k[j + 4].s.c_str());
            const Versioned* v = FindVersioned(kApiNames, sizeof(kApiNames) / sizeof(kApiNames[0]), name);
            const int mine = v ? (bClient ? v->client : v->server) : 0;
            const int theirs = v ? (bClient ? v->server : v->client) : 0;
            if (!mine && theirs) warning = "mod." + name + " is " + pOther + "'s; this file has no mod." + name;
            else if (!mine) warning = "mod." + name + " is not in the API (a misspelling?)";
            else { Use u = { "mod." + name, k[j].line, mine }; uses.push_back(u); }
            if (!warning.empty() && said.insert("mod." + name).second)
                lines.push_back(std::string(pPath) + ":" + std::to_string(k[j].line) + ": WARNING: " + warning);
            continue;
        }
        // require(...) or require "..." (API 7, MODULES), not a field's
        if (at.Name(j) && k[j].s == "require" && !(j > 0 && (at.Is(j - 1, ".") || at.Is(j - 1, ":"))) &&
            (at.Is(j + 1, "(") || (j + 1 < nk && k[j + 1].kind == 's')))
        {
            Use u = { "require", k[j].line, 7 };
            uses.push_back(u);
            continue;
        }
        // A global function: function OnX(...), or OnX = function.
        std::string def;
        if (at.Is(j, "function") && !(j > 0 && at.Is(j - 1, "local")) && at.Name(j + 1) && at.Is(j + 2, "("))
            def = k[j + 1].s;
        else if (at.Name(j) && at.Is(j + 1, "=") && at.Is(j + 2, "function") &&
                 !(j > 0 && (at.Is(j - 1, ".") || at.Is(j - 1, ":") || at.Is(j - 1, "local"))))
            def = k[j].s;
        const Versioned* v = def.empty() ? NULL : FindVersioned(kEventNames, sizeof(kEventNames) / sizeof(kEventNames[0]), def);
        const int mine = v ? (bClient ? v->client : v->server) : 0;
        if (mine) { Use u = { def, k[j].line, mine }; uses.push_back(u); }
    }
    // Above the version the script asks for, and the version it needs.
    int need = 0;
    size_t needAt = 0;
    for (size_t u = 0; u < uses.size(); ++u)
    {
        if (target && uses[u].version > target && said.insert("v:" + uses[u].what).second)
            lines.push_back(std::string(pPath) + ":" + std::to_string(uses[u].line) + ": WARNING: " + uses[u].what +
                            " came in mod.api " + std::to_string(uses[u].version) + ", above the mod.api " +
                            std::to_string(target) + " this script asks for with mod.require_api");
        if (uses[u].version > need) { need = uses[u].version; needAt = u; }
    }
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) out += lines[i] + "\n";
    if (need && !pModulePath)
        out += std::string(pPath) + ": needs mod.api " + std::to_string(need) + " (" + uses[needAt].what + ", line " +
               std::to_string(uses[needAt].line) + ")" +
               (target ? "" : "; mod.require_api(" + std::to_string(need) + ") would have an older game say so") + "\n";
    return out;
}

bool Script_Check(FileSource& src, ScriptSide side, void (*pPrint)(const char*))
{
    // Either side's game, with no world: nothing to draw on, no object.
    struct CheckHost : public ScriptClientHost
    {
        void (*pPrint)(const char*);
        virtual void Print(const char* p) { pPrint(p); }
        virtual void Command(const char* p) { pPrint((std::string("command: ") + p).c_str()); }
        virtual double Time() { return 0.0; }
        virtual bool DrawHudText(int, int, const char*, int, int, int) { return false; }
        virtual bool DrawRect(int, int, int, int, int, int, int) { return false; }
    };
    static CheckHost host;
    host.pPrint = pPrint;
    std::string sErr;
    const bool bOk = Script_Load(src, &host, sErr, side);
    if (!bOk)
        pPrint(sErr.c_str());
    else if (Script_Loaded())
    {
        const std::string sum = Script_Summary();
        for (size_t b = 0, e; b < sum.size(); b = e + 1)
        {
            e = sum.find('\n', b);
            if (e == std::string::npos) e = sum.size();
            pPrint((std::string(g_pPath) + ": " + sum.substr(b, e - b)).c_str());
        }        // And the static pass, over the same text.
        std::string text;
        src.ReadFile(g_pPath, text);
        std::string lint = Script_Lint(text, side);
        // And each module it required, under its own file's name
        for (size_t m = 0; m < g_ModuleOrder.size(); ++m)
        {
            std::string path = "Mod\\Scripts\\";
            for (size_t i = 0; i < g_ModuleOrder[m].size(); ++i)
                path += g_ModuleOrder[m][i] == '.' ? '\\' : g_ModuleOrder[m][i];
            path += ".lua";
            lint += Script_LintModule(g_ModuleText[g_ModuleOrder[m]], side, path.c_str());
        }
        for (size_t b = 0, e; b < lint.size(); b = e + 1)
        {
            e = lint.find('\n', b);
            if (e == std::string::npos) e = lint.size();
            if (e > b) pPrint(lint.substr(b, e - b).c_str());
        }
    }
    Script_Clear();
    return bOk;
}

bool Script_ClientMessage(const ScriptObject& player, const char* pName, const char* pPayload)
{
    if (!g_L || g_eSide != kScriptServer) return false;
    const size_t nName = pName ? strlen(pName) : 0, nPayload = pPayload ? strlen(pPayload) : 0;
    if (const char* why = Malformed(pName, nName, nPayload, false)) { Dropped(why); return false; }
    const std::string who = std::string("player ") + (player.pName && player.pName[0] ? player.pName : "(unnamed)");
    if (!Admit(player.hObj, who.c_str())) return false;
    const bool bHeard = PushHandler("OnClientMessage");
    if (bHeard) lua_pop(g_L, 1);
    // Raised either way: an unheard event still makes its player known.
    Raise("OnClientMessage", player, true, NULL, pName, pPayload ? pPayload : "");
    return bHeard;
}

bool Script_ServerMessage(const char* pName, const char* pPayload)
{
    if (!g_L || g_eSide != kScriptClient) return false;
    const size_t nName = pName ? strlen(pName) : 0, nPayload = pPayload ? strlen(pPayload) : 0;
    if (const char* why = Malformed(pName, nName, nPayload, false)) { Dropped(why); return false; }
    if (!PushHandler("OnServerMessage")) return false;
    lua_pushstring(g_L, pName);
    lua_pushstring(g_L, pPayload ? pPayload : "");
    if (!Call(2, "OnServerMessage", NULL)) Disable("OnServerMessage");
    return true;
}

int Script_MessagesDropped() { return g_nDropped; }

void Script_ClientStart()
{
    if (!PushHandler("OnClientStart")) return;
    if (!Call(0, "OnClientStart", NULL)) Disable("OnClientStart");
}

void Script_Draw(int nWidth, int nHeight)
{
    if (g_eSide != kScriptClient || !PushHandler("OnDraw")) return;
    lua_pushinteger(g_L, nWidth);
    lua_pushinteger(g_L, nHeight);
    g_bInDraw = true;
    const bool bOk = Call(2, "OnDraw", NULL, kScriptDrawBudget);
    g_bInDraw = false;
    if (!bOk) Disable("OnDraw");
}

} // namespace LT1Mod
