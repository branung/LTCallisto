#include "iltclient.h"
#include "iltsoundmgr.h"

#include "lt1mod_clientscript.h"
#include "lt1mod_engine.h"
#include "lt1mod_script.h"

#include <stdio.h>
#include <map>
#include <string>

namespace LT1Mod
{

namespace
{

ILTClient* s_pClient = NULL;
ClientScriptSendFn s_pSend = NULL;
std::string s_sWorld;       // the world being loaded, by its file name

class ClientScriptHost : public ScriptClientHost
{
public:
    ClientScriptHost() : m_hFont(NULL) {}

    void SetFace(const char* pFace) { m_sFace = pFace && pFace[0] ? pFace : "Arial"; }

    virtual void Print(const char* pLine)   { if (s_pClient) s_pClient->CPrint("%s", pLine); }
    virtual double Time()                   { return s_pClient ? s_pClient->GetTime() : 0.0; }

    // The font, made when the script loads rather than in its first OnDraw, where its cost
    // would count against the script
    bool Prepare()
    {
        if (!m_hFont && s_pClient) m_hFont = s_pClient->CreateFont(m_sFace.c_str(), 7, 14, LTFALSE, LTFALSE, LTFALSE);
        return m_hFont != NULL;
    }

    // A string's surface, made once per text and color and cached; the cache is emptied past
    // kMaxCached, which a script drawing changing text reaches
    virtual bool DrawHudText(int x, int y, const char* pText, int r, int g, int b)
    {
        if (!s_pClient || !pText || !Prepare()) return false;
        char key[32];
        snprintf(key, sizeof(key), "|%d,%d,%d", r & 255, g & 255, b & 255);
        const std::string k = std::string(pText) + key;
        std::map<std::string, HSURFACE>::iterator it = m_Cache.find(k);
        HSURFACE hText = it == m_Cache.end() ? NULL : it->second;
        if (!hText)
        {
            if (m_Cache.size() >= kMaxCached) Flush();
            HSTRING hStr = s_pClient->CreateString(const_cast<char*>(pText));
            if (!hStr) return false;
            hText = s_pClient->CreateSurfaceFromString(m_hFont, hStr, Color(r, g, b), 0, 0, 0);
            s_pClient->FreeString(hStr);
            if (!hText) return false;
            m_Cache[k] = hText;
        }
        s_pClient->DrawSurfaceToSurfaceTransparent(s_pClient->GetScreenSurface(), hText, NULL, x, y, 0);
        return true;
    }

    // The engine's FillRect on the screen, which clears the rectangle on the device
    virtual bool DrawRect(int x, int y, int w, int h, int r, int g, int b)
    {
        if (!s_pClient) return false;
        LTRect rc(x, y, x + w, y + h);
        s_pClient->FillRect(s_pClient->GetScreenSurface(), &rc, Color(r, g, b));
        return true;
    }

    virtual bool SendToServer(const char* pName, const char* pPayload)
    {
        return s_pSend && s_pSend(pName, pPayload);
    }

    virtual bool Sound2D(const char* pPath)
    {
        if (!s_pClient || !pPath || !s_pClient->SoundMgr()) return false;
        PlaySoundInfo psi;
        PLAYSOUNDINFO_INIT(psi);
        psi.m_dwFlags = PLAYSOUND_LOCAL;
        snprintf(psi.m_szSoundName, sizeof(psi.m_szSoundName), "%s", pPath);
        HLTSOUND hSnd = NULL;
        return s_pClient->SoundMgr()->PlaySound(&psi, hSnd) == LT_OK;
    }

private:
    static HLTCOLOR Color(int r, int g, int b)
    {
        return s_pClient->SetupColor1((r & 255) / 255.0f, (g & 255) / 255.0f, (b & 255) / 255.0f, LTFALSE);
    }

    void Flush()
    {
        for (std::map<std::string, HSURFACE>::iterator it = m_Cache.begin(); it != m_Cache.end(); ++it)
            if (s_pClient) s_pClient->DeleteSurface(it->second);
        m_Cache.clear();
    }

    static const size_t kMaxCached = 128;
    std::string m_sFace;
    HLTFONT m_hFont;
    std::map<std::string, HSURFACE> m_Cache;
};

ClientScriptHost s_Host;

void PrintLines(const std::string& s, const char* pPrefix)
{
    for (size_t b = 0, e; b < s.size(); b = e + 1)
    {
        e = s.find('\n', b);
        if (e == std::string::npos) e = s.size();
        s_pClient->CPrint("%s%s", pPrefix, s.substr(b, e - b).c_str());
    }
}

// `script <name> [args...]`: the client script's OnCommand
void ScriptConsoleFn(int argc, const char** argv)
{
    if (argc < 1) { s_pClient->CPrint("script <name> [args...]: calls the mod's ClientScript.lua OnCommand"); return; }
    if (!Script_Command(argv[0], argv + 1, argc - 1))
        s_pClient->CPrint("script %s: the mod's ClientScript.lua has no OnCommand", argv[0]);
}

} // namespace

void ClientScript_Load(ILTClient* pClient, const char* pFontFace, ClientScriptSendFn pSend)
{
    if (!pClient) return;
    s_pClient = pClient;
    s_pSend = pSend;
    s_Host.SetFace(pFontFace);
    EngineSource src(pClient, pClient->GetFileList, pClient->FreeFileList);
    std::string sErr;
    if (!Script_Load(src, &s_Host, sErr, kScriptClient))
    {
        PrintLines(sErr, "");
        return;
    }
    if (!Script_Loaded()) return;
    PrintLines(Script_Summary(), "Mod\\ClientScript.lua: ");
    s_Host.Prepare();
    pClient->RegisterConsoleProgram("script", ScriptConsoleFn);
    Script_ClientStart();
}

void ClientScript_PreLoadWorld(const char* pWorld)
{
    // A world's file name, as the server's script is given it
    s_sWorld = pWorld ? pWorld : "";
    const size_t slash = s_sWorld.find_last_of("\\/");
    if (slash != std::string::npos) s_sWorld = s_sWorld.substr(slash + 1);
    const size_t dot = s_sWorld.rfind('.');
    if (dot != std::string::npos && dot > 0) s_sWorld.erase(dot);   // Shogo's client is given "01_Ambush.dat"
}

void ClientScript_WorldStarted() { Script_WorldStart(s_sWorld.c_str()); }

void ClientScript_Update() { Script_Update(); }

void ClientScript_ServerMessage(const char* pName, const char* pPayload) { Script_ServerMessage(pName, pPayload); }

void ClientScript_Draw()
{
    if (!s_pClient || !Script_Loaded()) return;
    uint32 w = 0, h = 0;
    s_pClient->GetSurfaceDims(s_pClient->GetScreenSurface(), &w, &h);
    Script_Draw((int)w, (int)h);
}

} // namespace LT1Mod
