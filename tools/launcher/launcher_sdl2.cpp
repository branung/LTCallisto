#include "launcher_core.h"
#include "launcher_splash.h"

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <SDL_syswm.h>
#ifndef _WIN32
// SDL_syswm.h brings in X11's headers, whose None macro would break DialogId::None
#undef None
#endif

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../runtime/ui/src/stb_truetype.h"

#ifdef LTLAUNCH_UPDATES
#include "launcher_http.h"
#include "launcher_updater.h"
#include <memory>
#endif

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

// The game this build launches, set per launcher by CMake.
// Without one it's the combined launcher, LTCallisto, which lists every game in its Games folder
#ifndef LTLAUNCH_PROFILE
#define LTLAUNCH_COMBINED 1
#endif

// This build's stamp, <year>.<month><day>.<time>, written on every build by stamp.cmake
extern const char *g_szLauncherBuild;

namespace {

// The launcher's name, shown with its build stamp
const char *kLauncherName = "LTCallisto";

// Launcher color palette (0xRRGGBB)
const Uint32 kFace   = 0xF0F0F0;
const Uint32 kWindow = 0xFFFFFF;
const Uint32 kBorder = 0x7A7A7A;
const Uint32 kButton = 0xE1E1E1;
const Uint32 kButtonDown = 0xCCE4F7;
const Uint32 kBorderDown = 0x005499;
const Uint32 kText   = 0x000000;
const Uint32 kAlert  = 0xA00000;
const Uint32 kGrayTxt = 0x6D6D6D;
const Uint32 kCaption = 0x0078D7;
const Uint32 kCapText = 0xFFFFFF;
#ifdef LTLAUNCH_UPDATES
const Uint32 kLink = 0x0066CC;
#endif

// The combined launcher's games list, left of the game's own area. It can be hidden, which narrows the window
#ifdef LTLAUNCH_COMBINED
const int kGameListW = 185;
#else
const int kGameListW = 0;
#endif

// The banner. The games' art is 4:3, so it's drawn at half of 640x480, which scales it cleanly,
// inside a 1 pixel border. The rest of the window is laid out around it
const int kBannerW = 320, kBannerH = 240;
const int kPanelY = 40, kPanelW = kBannerW + 2, kPanelH = kBannerH + 2;
const int kPanelBottom = kPanelY + kPanelH;

// The column of buttons right of the banner, 12 pixels from it and from the window's edge.
// Wide enough for "Launch Nightmares"
const int kButtonColumnW = 140;

// The game's own area: the banner and the buttons
const int kMainW = 12 + kPanelW + 12 + kButtonColumnW + 12;

// Below the banner: the status line, the update notice and the footer, then the window's bottom edge
const int kStatusY = kPanelBottom + 36;
const int kFooterY = kPanelBottom + 110;
const int kWinH = kPanelBottom + 140;

// Set by Layout: the games list's width, 0 while it's hidden, and the window's width
int g_nSideW = 0;
int g_nWinW = kMainW;

struct Rect
{
    int x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int a, int b, int c, int d) : x(a), y(b), w(c), h(d) {}
};

bool In(const Rect &r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// Everything left to right moves with the games list, so these are set by Layout.
// Quit's top lines up with the banner's bottom, and the addon's checkbox sits under the banner level with Quit
Rect kBannerPanel, kPlayButton, kQuitButton, kDisplayButton, kCustomizeButton, kAddOnBox;

// The folder row spans the banner's width: the games list's toggle in the combined launcher, the folder, Browse...
const int kBrowseW = 74;
const int kToggleW = kGameListW > 0 ? 22 : 0;
Rect kGameListToggle, kDataDirBox, kBrowseButton;

// The games list's heading, and its buttons spaced like Display... and Customize...
Rect kGamesHeading;
const int  kGameButtonY = 52, kGameButtonStep = 34;

const int kCaptionH = 22;
const int kCloseSize = 18;
const int kDlgButtonW = 80, kDlgButtonH = 24;
const int kDlgButtonGap = 8; // Gap between OK and Cancel and between Cancel and the frame's edge
const int kDlgBottomMargin = 10; // Margin below OK and Cancel

// A dialog centered on the launcher, with its caption bar, close box, OK and Cancel
struct Dialog
{
    Rect frame, caption, close, ok, cancel;

    Dialog() {}
    Dialog(int w, int h)
        : frame((g_nWinW - w) / 2, (kWinH - h) / 2, w, h),
          caption(frame.x + 1, frame.y + 1, w - 2, kCaptionH),
          close(caption.x + caption.w - kCloseSize - 3, caption.y + 2, kCloseSize, kCloseSize),
          ok(frame.x + w - 2 * (kDlgButtonW + kDlgButtonGap), frame.y + h - kDlgButtonH - kDlgBottomMargin,
             kDlgButtonW, kDlgButtonH),
          cancel(frame.x + w - (kDlgButtonW + kDlgButtonGap), frame.y + h - kDlgButtonH - kDlgBottomMargin,
                 kDlgButtonW, kDlgButtonH) {}
};

// The dialogs, narrow enough for the window with the games list hidden
Dialog kDisplayDlg;
Rect kSizeList, kBorderlessBox;

Dialog kCustomizeDlg;
Rect kAvailableList, kChosenList, kAddButton, kRemoveButton, kAlwaysRezBox;

#ifdef LTLAUNCH_UPDATES
// A release's notes, opened from the update notice above the footer. Its buttons update and restart,
// with the download's progress or an error on the line above them
Dialog kReleaseDlg;
const int kUpdateStatusH = 24;
Rect kNotesBox, kGitHubButton;
#endif

// Places everything for the games list shown or hidden
void Layout(bool bGameList)
{
    g_nSideW = bGameList ? kGameListW : 0;
    g_nWinW = g_nSideW + kMainW;

    const int x = g_nSideW + 12;
    const int nColumnX = x + kPanelW + 12;
    kBannerPanel = Rect(x, kPanelY, kPanelW, kPanelH);
    kPlayButton = Rect(nColumnX, 12, kButtonColumnW, 26);
    kDisplayButton = Rect(nColumnX, 52, kButtonColumnW, 26);
    kCustomizeButton = Rect(nColumnX, 86, kButtonColumnW, 26);
    kQuitButton = Rect(nColumnX, kPanelBottom, kButtonColumnW, 26);
    kAddOnBox = Rect(x, kPanelBottom + 8, kPanelW, 18);

    kGameListToggle = Rect(x, 12, kToggleW, 22);
    const int nDirX = kToggleW > 0 ? x + kToggleW + 6 : x;
    kBrowseButton = Rect(x + kPanelW - kBrowseW, 12, kBrowseW, 22);
    kDataDirBox = Rect(nDirX, 12, kBrowseButton.x - 6 - nDirX, 22);
    kGamesHeading = Rect(12, 12, kGameListW - 24, 26);

    kDisplayDlg = Dialog(440, 320);
    const Rect &d = kDisplayDlg.frame;
    kSizeList = Rect(d.x + 14, d.y + 48, d.w - 28, 172);
    kBorderlessBox = Rect(d.x + 14, d.y + 228, d.w - 28, 18);

    kCustomizeDlg = Dialog(470, 290);
    const Rect &c = kCustomizeDlg.frame;
    kAvailableList = Rect(c.x + 14, c.y + 46, 165, 160);
    kAddButton = Rect(c.x + 190, c.y + 90, 90, 24);
    kRemoveButton = Rect(c.x + 190, c.y + 122, 90, 24);
    kChosenList = Rect(c.x + c.w - 14 - 165, c.y + 46, 165, 160);
    kAlwaysRezBox = Rect(c.x + (c.w - 200) / 2, c.y + 216, 200, 18);

#ifdef LTLAUNCH_UPDATES
    kReleaseDlg = Dialog(470, 340);
    const Rect &r = kReleaseDlg.frame;
    kNotesBox = Rect(r.x + 14, r.y + kCaptionH + 12, r.w - 28,
                     r.h - kCaptionH - 12 - kDlgButtonH - kDlgBottomMargin - 10 - kUpdateStatusH);
    kGitHubButton = Rect(r.x + 14, kReleaseDlg.ok.y, 130, kDlgButtonH);
#endif
}

#ifdef LTLAUNCH_UPDATES
// The update notice's line, between the status line and the footer
const int kUpdateY = kFooterY - 36;

// The notes' scroll bar arrows, and how a held one repeats (ms)
const int kScrollArrowH = 14;
const Uint32 kArrowDelay = 400, kArrowRepeat = 50;
#endif
const int  kRowH = 18;
const int  kListPad = 2;
const int  kScrollBarW = 14;
const int  kMinHandleH = 12;
const int  kCheckSize = 13;

SDL_Renderer *g_pRen  = 0;
SDL_Texture  *g_pFont = 0;

const int   kFontFirst = 32, kFontCount = 95;
const float kFontPx    = 16.0f;

stbtt_bakedchar g_Glyphs[kFontCount];
int g_nAscent = 0, g_nLineH = 0;

// Drawing

void SetColor(Uint32 c)
{
    SDL_SetRenderDrawColor(g_pRen, (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255);
}

void Fill(const Rect &r, Uint32 c)
{
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SetColor(c);
    SDL_RenderFillRect(g_pRen, &sr);
}

void Frame(const Rect &r, Uint32 c)
{
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SetColor(c);
    SDL_RenderDrawRect(g_pRen, &sr);
}

int GlyphIndex(int ch)
{
    if (ch < kFontFirst || ch >= kFontFirst + kFontCount) ch = '?';
    return ch - kFontFirst;
}

int TextW(const std::string &s)
{
    float w = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) w += g_Glyphs[GlyphIndex((unsigned char)s[i])].xadvance;
    return (int)(w + 0.5f);
}

void Text(int x, int y, const std::string &s, Uint32 c, int nMaxW = 100000)
{
    if (!g_pFont) return;
    SDL_SetTextureColorMod(g_pFont, (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c);
    float pen = (float)x;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const stbtt_bakedchar &b = g_Glyphs[GlyphIndex((unsigned char)s[i])];
        if (pen + b.xadvance > (float)(x + nMaxW)) break;
        SDL_Rect src = { b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0 };
        if (src.w > 0 && src.h > 0)
        {
            SDL_Rect dst = { (int)(pen + 0.5f) + (int)b.xoff, y + g_nAscent + (int)b.yoff,
                             src.w, src.h };
            SDL_RenderCopy(g_pRen, g_pFont, &src, &dst);
        }
        pen += b.xadvance;
    }
}

void TextC(const Rect &r, const std::string &s, Uint32 c)
{
    Text(r.x + (r.w - TextW(s)) / 2, r.y + (r.h - g_nLineH) / 2, s, c, r.w);
}

// A path that's too wide keeps its end, which names the folder.
// TextW rounds, so a fit is strictly narrower: one that only fits when rounded lost its last character to Text's clip
std::string FitLeft(const std::string &s, int nMaxW)
{
    if (TextW(s) < nMaxW) return s;
    size_t i = 0;
    while (i < s.size() && TextW("..." + s.substr(i)) >= nMaxW) ++i;
    return "..." + s.substr(i);
}

// Tahoma from the system's fonts. Has fallback attempts on Linux in case it's not available.
bool ReadFontFile(std::vector<unsigned char> &data)
{
    std::vector<std::string> paths;
#ifdef _WIN32
    const char *pWinDir = SDL_getenv("WINDIR");
    paths.push_back(std::string(pWinDir ? pWinDir : "C:\\Windows") + "\\Fonts\\tahoma.ttf");
#else
    paths.push_back("/usr/share/wine/fonts/tahoma.ttf");
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/dejavu/DejaVuSans.ttf");
#endif
    for (size_t i = 0; i < paths.size(); ++i)
    {
        std::ifstream in(paths[i].c_str(), std::ios::binary | std::ios::ate);
        if (!in.good()) continue;
        const std::streamsize nSize = in.tellg();
        if (nSize <= 0) continue;
        data.resize((size_t)nSize);
        in.seekg(0);
        if (in.read((char *)&data[0], nSize)) return true;
    }
    return false;
}

SDL_Texture *MakeFontAtlas()
{
    std::vector<unsigned char> ttf;
    if (!ReadFontFile(ttf)) return 0;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) return 0;
    const float scale = stbtt_ScaleForPixelHeight(&info, kFontPx);
    int nAscent, nDescent, nGap;
    stbtt_GetFontVMetrics(&info, &nAscent, &nDescent, &nGap);
    g_nAscent = (int)(nAscent * scale + 0.5f);
    g_nLineH  = (int)((nAscent - nDescent) * scale + 0.5f);

    const int w = 512, h = 256;
    std::vector<unsigned char> alpha((size_t)w * h);
    if (stbtt_BakeFontBitmap(&ttf[0], 0, kFontPx, &alpha[0], w, h,
                             kFontFirst, kFontCount, g_Glyphs) <= 0)
        return 0;

    SDL_Surface *pSurf = SDL_CreateRGBSurface(0, w, h, 32,
                                              0x00FF0000, 0x0000FF00, 0x000000FF,
                                              0xFF000000);
    if (!pSurf) return 0;
    SDL_LockSurface(pSurf);
    for (int y = 0; y < h; ++y)
    {
        Uint32 *pRow = (Uint32 *)((Uint8 *)pSurf->pixels + y * pSurf->pitch);
        for (int x = 0; x < w; ++x)
            pRow[x] = ((Uint32)alpha[(size_t)y * w + x] << 24) | 0x00FFFFFFu;
    }
    SDL_UnlockSurface(pSurf);
    SDL_Texture *pTex = SDL_CreateTextureFromSurface(g_pRen, pSurf);
    SDL_FreeSurface(pSurf);
    if (pTex) SDL_SetTextureBlendMode(pTex, SDL_BLENDMODE_BLEND);
    return pTex;
}

void Button(const Rect &r, const std::string &sLabel, bool bPressed = false)
{
    Fill(r, bPressed ? kButtonDown : kButton);
    Frame(r, bPressed ? kBorderDown : kBorder);
    TextC(bPressed ? Rect(r.x + 1, r.y + 1, r.w, r.h) : r, sLabel, kText);
}

void CheckBox(const Rect &hit, const std::string &sLabel, bool bOn, bool bPressed)
{
    const Rect box(hit.x, hit.y + (hit.h - kCheckSize) / 2, kCheckSize, kCheckSize);
    Fill(box, bPressed ? kButtonDown : kWindow);
    Frame(box, bPressed ? kBorderDown : kBorder);
    if (bOn)
    {
        for (int i = 0; i < 3; ++i) Fill(Rect(box.x + 3 + i, box.y + 5 + i, 1, 3), kText);
        for (int i = 0; i < 4; ++i) Fill(Rect(box.x + 6 + i, box.y + 7 - i, 1, 3), kText);
    }
    Text(box.x + kCheckSize + 6, hit.y + (hit.h - g_nLineH) / 2, sLabel, kText);
}

int VisibleRows(const Rect &r)
{
    return (r.h - 2 * kListPad) / kRowH;
}

void ListBox(const Rect &r, const std::vector<std::string> &items, int iSel, int nScroll)
{
    Fill(r, kWindow);
    Frame(r, kBorder);
    const int nVis = VisibleRows(r);
    const int nItems = (int)items.size();
    const bool bBar = nItems > nVis;
    const int nRowW = r.w - 2 * kListPad - (bBar ? kScrollBarW : 0);

    for (int n = 0; n < nVis && nScroll + n < nItems; ++n)
    {
        const int i = nScroll + n;
        const Rect row(r.x + kListPad, r.y + kListPad + n * kRowH, nRowW, kRowH);
        if (i == iSel) Fill(row, kCaption);
        Text(row.x + 4, row.y + (kRowH - g_nLineH) / 2, items[(size_t)i],
             i == iSel ? kCapText : kText, row.w - 8);
    }

    if (bBar)
    {
        const Rect bar(r.x + r.w - kScrollBarW, r.y + kListPad, kScrollBarW - kListPad, r.h - 2 * kListPad);
        Fill(bar, kFace);

        int nHandleH = bar.h * nVis / nItems;
        if (nHandleH < kMinHandleH) nHandleH = kMinHandleH;
        const int nTravel = bar.h - nHandleH;
        const int nHandleY = bar.y + nTravel * nScroll / (nItems - nVis);
        const Rect handle(bar.x, nHandleY, bar.w, nHandleH);
        Fill(handle, kButton);
        Frame(handle, kBorder);
    }
}

int ListRowAt(const Rect &list, int nItems, int nScroll, int x, int y)
{
    const int nVis = VisibleRows(list);
    const int nBar = (nItems > nVis) ? kScrollBarW : 0;
    const Rect rows(list.x + kListPad, list.y + kListPad, list.w - 2 * kListPad - nBar, nVis * kRowH);
    if (!In(rows, x, y)) return -1;
    return nScroll + (y - rows.y) / kRowH;
}

int ClampScroll(const Rect &list, int nItems, int nScroll)
{
    const int nMax = nItems - VisibleRows(list);
    if (nScroll > nMax) nScroll = nMax;
    if (nScroll < 0) nScroll = 0;
    return nScroll;
}

std::vector<ltlaunch::WindowSize> DesktopSizes()
{
    std::vector<ltlaunch::WindowSize> all;
    for (int d = 0; d < SDL_GetNumVideoDisplays(); ++d)
        for (int m = 0; m < SDL_GetNumDisplayModes(d); ++m)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDisplayMode(d, m, &dm) != 0) continue;
            all.push_back({ dm.w, dm.h });
        }
    return ltlaunch::DistinctSizes(all);
}

// Asks for a folder, starting in sStart. False when cancelled, with sError set when it couldn't ask
#ifdef _WIN32
bool PickFolder(SDL_Window *pWnd, const std::string &sStart, std::string &sOut, std::string &sError)
{
    HWND hOwner = NULL;
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (SDL_GetWindowWMInfo(pWnd, &wm)) hOwner = wm.info.win.window;

    const HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool bPicked = false;
    IFileOpenDialog *pDlg = NULL;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg))))
    {
        sError = "Could not open the folder dialog";
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return false;
    }

    DWORD dwOptions = 0;
    pDlg->GetOptions(&dwOptions);
    pDlg->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    pDlg->SetTitle(L"Choose the folder holding the game's .rez files");

    wchar_t wStart[MAX_PATH];
    IShellItem *pStart = NULL;
    if (MultiByteToWideChar(CP_ACP, 0, sStart.c_str(), -1, wStart, MAX_PATH) &&
        SUCCEEDED(SHCreateItemFromParsingName(wStart, NULL, IID_PPV_ARGS(&pStart))))
    {
        pDlg->SetFolder(pStart);
        pStart->Release();
    }

    IShellItem *pItem = NULL;
    if (SUCCEEDED(pDlg->Show(hOwner)) && SUCCEEDED(pDlg->GetResult(&pItem)))
    {
        PWSTR pPath = NULL;
        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)))
        {
            // The engine takes ANSI paths, so a name the code page can't hold is refused
            char sPath[MAX_PATH * 2];
            BOOL bLossy = FALSE;
            if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, pPath, -1, sPath, sizeof(sPath), NULL, &bLossy) &&
                !bLossy)
            {
                sOut = sPath;
                bPicked = true;
            }
            else
                sError = "That folder's name has characters the engine can't open";
            CoTaskMemFree(pPath);
        }
        pItem->Release();
    }

    pDlg->Release();
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return bPicked;
}
#else
std::string ShellQuote(const std::string &s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

// zenity, or kdialog when it isn't installed
bool PickFolder(SDL_Window *, const std::string &sStart, std::string &sOut, std::string &sError)
{
    const std::string kCmds[] = {
        "zenity --file-selection --directory --title='Choose the game data folder' --filename=" +
            ShellQuote(ltlaunch::JoinPath(sStart, "")),
        "kdialog --getexistingdirectory " + ShellQuote(sStart),
    };
    for (const std::string &sCmd : kCmds)
    {
        FILE *pPipe = popen((sCmd + " 2>/dev/null").c_str(), "r");
        if (!pPipe) continue;
        std::string sLine;
        char buf[512];
        while (fgets(buf, sizeof(buf), pPipe)) sLine += buf;
        const int nStatus = pclose(pPipe);
        while (!sLine.empty() && (sLine.back() == '\n' || sLine.back() == '\r')) sLine.pop_back();

        if (WIFEXITED(nStatus) && WEXITSTATUS(nStatus) == 127) continue; // Not installed
        if (WIFEXITED(nStatus) && WEXITSTATUS(nStatus) == 0 && !sLine.empty())
        {
            sOut = sLine;
            return true;
        }
        return false; // Cancelled
    }
    sError = "Install zenity or kdialog to choose a folder";
    return false;
}
#endif

// A started process, kept to ask later whether it's still running
#ifdef _WIN32
typedef HANDLE ProcessRef;
#else
typedef pid_t ProcessRef;
#endif

// Starts the engine with the game directory as its working directory.
// pProcess keeps the process when it's given
#ifdef _WIN32
bool Spawn(const std::vector<std::string> &args, const std::string &sCwd,
           std::string &sError, ProcessRef *pProcess = 0)
{
    std::string sCmd = ltlaunch::QuoteArgs(args);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    std::vector<char> cmd(sCmd.begin(), sCmd.end()); cmd.push_back('\0');
    if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL,
                        sCwd.c_str(), &si, &pi))
    { sError = "Could not start " + args[0]; return false; }
    CloseHandle(pi.hThread);
    if (pProcess) *pProcess = pi.hProcess;
    else CloseHandle(pi.hProcess);
    return true;
}
#else
bool Spawn(const std::vector<std::string> &args, const std::string &sCwd,
           std::string &sError, ProcessRef *pProcess = 0)
{
    const pid_t pid = fork();
    if (pid < 0) { sError = "Could not start " + args[0]; return false; }
    if (pid == 0)
    {
        if (chdir(sCwd.c_str()) != 0) _exit(127);
        std::vector<const char *> cargv;
        for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());
        cargv.push_back(0);
        execv(cargv[0], (char *const *)&cargv[0]);
        _exit(127);
    }
    if (pProcess) *pProcess = pid;
    return true;
}
#endif

#ifdef LTLAUNCH_UPDATES
// Whether a started process is still running. A finished one is released
bool StillRunning(ProcessRef p)
{
#ifdef _WIN32
    if (WaitForSingleObject(p, 0) == WAIT_TIMEOUT) return true;
    CloseHandle(p);
    return false;
#else
    int nStatus = 0;
    return waitpid(p, &nStatus, WNOHANG) == 0;
#endif
}

// Lets go of a process that's still running when the launcher closes
void ReleaseProcess(ProcessRef p)
{
#ifdef _WIN32
    CloseHandle(p);
#else
    (void)p;
#endif
}
#endif

struct App
{
    // Every game this launcher lists, the shown one copied into game and spec
    std::vector<ltlaunch::GameProfile> games;
    std::vector<Rect> gameButtons;
    int iGame = -1;
    std::string sSelectedPath; // The combined launcher's own launcher.ini, naming the selected game

    // A game's choices while another is shown, so a session's custom archives survive switching
    struct GameState
    {
        bool bSeen = false;
        std::vector<std::string> chosenRez;
        bool bAlwaysRez = false, bAddOn = false;
        std::string sDataDir;
    };
    std::vector<GameState> states;

    ltlaunch::GameProfile game;
    ltlaunch::LaunchSpec spec;
    std::string sStatus;
    bool bQuit = false;

    enum class DialogId { None, Display, Customize, Release };
    DialogId eOpenDialog = DialogId::None;

#ifdef LTLAUNCH_UPDATES
    std::string sExeDir;
    std::unique_ptr<ltlaunch::Updater> pUpdater; // Null when updates are off
    ltlaunch::Updater::View update; // Taken on each Draw
    Rect rUpdateLink; // Laid out on each Draw, empty when not shown
    std::string sUpdateMessage; // Why Restart was refused, shown in the release notes
    std::vector<ProcessRef> gameProcesses; // The games this launcher started

    // The release notes, wrapped to the dialog
    struct NoteRow
    {
        std::string sText;
        int x = 0;
        bool bHeading = false, bBullet = false;
    };
    std::vector<NoteRow> noteRows;
    int nNotesScroll = 0;
    bool bNotesDrag = false; // The scroll bar's handle is held
    int nNotesGrabY = 0; // Where on the handle it was taken
    int nNotesArrow = 0; // The arrow held, -1 up or 1 down
    Uint32 nArrowRepeatAt = 0; // When a held arrow next scrolls
#endif

    const Rect *pPressed = 0;
    bool bPressedOver = false;

    SDL_Texture *pSplash = 0;
    bool bSplashTried = false;

    SDL_Window *pWnd = 0; // The folder dialog's owner

    std::vector<ltlaunch::WindowSize> sizes;
    int  iSize = -1, iSizeOpen = -1;
    bool bBorderless = false, bBorderlessOpen = false;
    int  nSizeScroll = 0;

    // Extra archives loaded after the game's own
    // The 'OnOpen' copy is what Cancel puts back
    std::vector<std::string> availableRez, chosenRez, chosenRezOnOpen;
    bool bAlwaysRez = false, bAlwaysRezOnOpen = false;
    std::string sSettingsPath;
    int iAvailableSel = -1, iChosenSel = -1;
    int nAvailableScroll = 0, nChosenScroll = 0;

    // The addon's checkbox, shown only when its modules are built
    bool bHasAddOn = false, bAddOn = false;

    bool UseAddOn() const { return bHasAddOn && bAddOn; }

    // Shows a game. Each keeps its own settings in launcher.ini in its folder, which for a single
    // game's launcher is the launcher's own folder
    void SelectGame(int i)
    {
        if (i < 0 || i >= (int)games.size() || i == iGame) return;
        if (iGame >= 0)
        {
            GameState &old = states[(size_t)iGame];
            old.bSeen = true;
            old.chosenRez = chosenRez;
            old.bAlwaysRez = bAlwaysRez;
            old.bAddOn = bAddOn;
            old.sDataDir = spec.m_sDataDir;
        }

        iGame = i;
        game = games[(size_t)i];
        spec = game.m_Spec;
        bHasAddOn = ltlaunch::HasAddOn(game);
        sSettingsPath = ltlaunch::JoinPath(spec.m_sGameDir, "launcher.ini");

        const GameState &st = states[(size_t)i];
        if (st.bSeen)
        {
            chosenRez = st.chosenRez;
            bAlwaysRez = st.bAlwaysRez;
            bAddOn = st.bAddOn;
            spec.m_sDataDir = st.sDataDir;
        }
        else
        {
            chosenRez.clear();
            bAlwaysRez = bAddOn = false;
            LoadSettings();
        }

        bSplashTried = false;
        sStatus.clear();
        if (!sSelectedPath.empty()) ltlaunch::SaveSelectedGame(sSelectedPath, game.m_sKey);
    }

    // An archive the addon loads, named from its own folder.
    // Its folder is usually inside the game's, but the combined launcher's is beside it
    std::string FromAddOn(const std::string &sPath) const
    {
        if (ltlaunch::IsAbsolutePath(sPath)) return sPath;
        if (ltlaunch::IsAbsolutePath(game.m_AddOn.m_sDir)) return ltlaunch::JoinPath(spec.m_sGameDir, sPath);
        return ltlaunch::JoinPath("..", sPath);
    }

    // Every archive this launch can't start without
    std::vector<std::string> Required() const
    {
        std::vector<std::string> out = ltlaunch::RequiredRez(spec);
        if (UseAddOn()) out.insert(out.end(), game.m_AddOn.m_Rez.begin(), game.m_AddOn.m_Rez.end());
        return out;
    }

    bool HasData(const std::string &sRez) const
    {
        return ltlaunch::FileExists(ltlaunch::JoinPath(ltlaunch::DataDir(spec), sRez));
    }

    void AddRezArgs(std::vector<std::string> &rez, const std::vector<std::string> &names) const
    {
        for (const std::string &sRez : names) rez.push_back(ltlaunch::RezArg(spec, sRez));
    }

    void AddPresent(std::vector<std::string> &rez, const std::vector<std::string> &optional) const
    {
        for (const std::string &sRez : optional)
            if (HasData(sRez)) rez.push_back(ltlaunch::RezArg(spec, sRez));
    }

    // The archives a launch loads. The game's come from the data folder, and Custom's from the game folder.
    // Order: the game's archives and whichever optional ones are present, then the addon's,
    // then the Custom folder itself so loose levels are listed ingame, then the selected custom archives.
    // False when a selected archive is no longer there, with its name stored in sMissing
    bool LaunchRez(std::vector<std::string> &rez, std::string &sMissing) const
    {
        rez.clear();
        AddRezArgs(rez, spec.m_Rez);
        AddPresent(rez, spec.m_OptionalRez);

        if (UseAddOn())
        {
            AddRezArgs(rez, game.m_AddOn.m_Rez);
            AddPresent(rez, game.m_AddOn.m_OptionalRez);
        }

        if (ltlaunch::DirExists(ltlaunch::JoinPath(spec.m_sGameDir, ltlaunch::kCustomFolder)))
            rez.push_back(ltlaunch::kCustomFolder);

        for (const std::string &sRez : chosenRez)
        {
            const std::string sPath = ltlaunch::JoinPath(spec.m_sGameDir, sRez);
            if (!ltlaunch::FileExists(sPath) && !ltlaunch::DirExists(sPath))
            {
                sMissing = sRez;
                return false;
            }
            rez.push_back(sRez);
        }
        return true;
    }

    // The engine saves autoexec.cfg where it runs, so the addon keeps its own, starting from a copy of the game's
    void SeedAddOnConfig() const
    {
        const std::string sOwn = ltlaunch::JoinPath(ltlaunch::AddOnDir(game), "autoexec.cfg");
        const std::string sGame = ltlaunch::JoinPath(spec.m_sGameDir, "autoexec.cfg");
        if (!ltlaunch::FileExists(sOwn) && ltlaunch::FileExists(sGame))
            ltlaunch::WriteTextFile(sOwn, ltlaunch::ReadTextFile(sGame));
    }

    void DoPlay()
    {
        if (!ltlaunch::FirstMissing(ltlaunch::DataDir(spec), Required()).empty()) return;

        ltlaunch::LaunchSpec launch = spec;
        std::string sMissing;
        if (!LaunchRez(launch.m_Rez, sMissing))
        {
            sStatus = sMissing + " was not found";
            return;
        }

        // The addon runs from its own folder
        std::string sCwd = spec.m_sGameDir;
        if (UseAddOn())
        {
            for (std::string &sRez : launch.m_Rez) sRez = FromAddOn(sRez);
            sCwd = ltlaunch::AddOnDir(game);
            SeedAddOnConfig();
        }

        std::vector<std::string> args = ltlaunch::BuildCommandLine(launch);

        // The engine never writes display.cfg, so the addon reads the game's
        if (UseAddOn() && ltlaunch::FileExists(DisplayCfgPath()))
        {
            args.push_back("-display");
            args.push_back(FromAddOn("display.cfg"));
        }

        std::string sError;
#ifdef LTLAUNCH_UPDATES
        ProcessRef process = 0;
        if (!Spawn(args, sCwd, sError, &process))
        { sStatus = sError; return; }
        gameProcesses.push_back(process);
#else
        if (!Spawn(args, sCwd, sError))
        { sStatus = sError; return; }
#endif
        sStatus.clear();
    }

    bool Down(const Rect &r) const { return pPressed == &r && bPressedOver; }

    // The buttons a screen takes clicks on
    static const std::vector<const Rect *> &ButtonsFor(DialogId eDialog)
    {
        static const std::vector<const Rect *> kMain =
            { &kPlayButton, &kDisplayButton, &kCustomizeButton, &kQuitButton, &kAddOnBox, &kBrowseButton,
              &kGameListToggle };
        static const std::vector<const Rect *> kDisplay =
            { &kDisplayDlg.ok, &kDisplayDlg.cancel, &kDisplayDlg.close, &kBorderlessBox };
        static const std::vector<const Rect *> kCustomize =
            { &kCustomizeDlg.ok, &kCustomizeDlg.cancel, &kCustomizeDlg.close, &kAddButton, &kRemoveButton,
              &kAlwaysRezBox };
#ifdef LTLAUNCH_UPDATES
        static const std::vector<const Rect *> kRelease =
            { &kReleaseDlg.ok, &kReleaseDlg.cancel, &kReleaseDlg.close, &kGitHubButton };
#endif

        switch (eDialog)
        {
            case DialogId::Display:   return kDisplay;
            case DialogId::Customize: return kCustomize;
#ifdef LTLAUNCH_UPDATES
            case DialogId::Release:   return kRelease;
#endif
            default:                  return kMain;
        }
    }

    const Rect *ButtonAt(int x, int y) const
    {
        for (const Rect *pButton : ButtonsFor(eOpenDialog))
        {
#ifdef LTLAUNCH_UPDATES
            if (pButton == &kReleaseDlg.ok && UpdateLabel().empty()) continue;
#endif
            if (In(*pButton, x, y) && (pButton != &kAddOnBox || bHasAddOn)) return pButton;
        }
        if (eOpenDialog == DialogId::None)
        {
            if (g_nSideW > 0)
                for (const Rect &r : gameButtons)
                    if (In(r, x, y)) return &r;
#ifdef LTLAUNCH_UPDATES
            if (In(rUpdateLink, x, y)) return &rUpdateLink;
#endif
        }
        return 0;
    }

    // The game a list button shows, or -1 when it isn't one
    int GameButtonIndex(const Rect *p) const
    {
        for (size_t i = 0; i < gameButtons.size(); ++i)
            if (p == &gameButtons[i]) return (int)i;
        return -1;
    }

    static bool IsOkButton(const Rect *p) { return p == &kDisplayDlg.ok || p == &kCustomizeDlg.ok; }

    void Press(const Rect *p)
    {
        // The game list
        if (GameButtonIndex(p) >= 0) SelectGame(GameButtonIndex(p));
        // Main window
        else if (p == &kPlayButton) DoPlay();
        else if (p == &kDisplayButton) OpenDisplay();
        else if (p == &kCustomizeButton) OpenCustomize();
        else if (p == &kQuitButton) bQuit = true;
        else if (p == &kAddOnBox) ToggleAddOn();
        else if (p == &kBrowseButton) BrowseDataDir();
        else if (p == &kGameListToggle) ToggleGameList();
        // Display dialog
        else if (p == &kBorderlessBox) bBorderless = !bBorderless;
        // Customize dialog
        else if (p == &kAddButton) AddRez();
        else if (p == &kRemoveButton) RemoveRez();
        else if (p == &kAlwaysRezBox) bAlwaysRez = !bAlwaysRez;
#ifdef LTLAUNCH_UPDATES
        // The update notice and the release notes
        else if (p == &rUpdateLink) OpenReleaseNotes();
        else if (p == &kReleaseDlg.ok) UpdateAction(); // The notes stay open to show the progress
        else if (p == &kGitHubButton) OpenReleasePage();
#endif
        // OK, Cancel or a close box. Cancel and the close boxes put things back
        else CloseDialog(IsOkButton(p));
    }

#ifdef LTLAUNCH_UPDATES
    // The release notes' button for the next step. Empty while there's nothing to press
    std::string UpdateLabel() const
    {
        switch (update.m_eState)
        {
            case ltlaunch::Updater::State::Available: return "Update";
            case ltlaunch::Updater::State::Ready:     return "Restart";
            case ltlaunch::Updater::State::Failed:    return "Retry";
            default:                                  return std::string();
        }
    }

    void UpdateAction()
    {
        if (!pUpdater) return;
        update = pUpdater->Get();
        sUpdateMessage.clear();
        switch (update.m_eState)
        {
            case ltlaunch::Updater::State::Available:
            case ltlaunch::Updater::State::Failed: pUpdater->StartDownload(); break;
            case ltlaunch::Updater::State::Ready:  RestartToUpdate(); break;
            default: break;
        }
    }

    void OpenReleasePage()
    {
        if (!pUpdater) return;
        if (!ltlaunch::OpenInBrowser(update.m_Release.m_sPageUrl) &&
            !ltlaunch::OpenInBrowser(pUpdater->ReleasesPage()))
            sUpdateMessage = "Could not open the release page";
    }

    // Whether a game this launcher started is still running
    bool GameRunning()
    {
        bool bRunning = false;
        for (size_t i = gameProcesses.size(); i-- > 0;)
        {
            if (StillRunning(gameProcesses[i])) { bRunning = true; continue; }
            gameProcesses.erase(gameProcesses.begin() + (std::ptrdiff_t)i);
        }
        return bRunning;
    }

    // Installs the downloaded update, then starts the new launcher and closes this one
    void RestartToUpdate()
    {
        if (GameRunning())
        {
            sUpdateMessage = "Close the game before restarting to update.";
            return;
        }
        if (!pUpdater->Install()) return; // The release notes say why

        std::string sError;
        if (Spawn({ ltlaunch::JoinPath(sExeDir, ltlaunch::kLauncherFile) }, sExeDir, sError)) bQuit = true;
        else sUpdateMessage = "Updated! Start LTCallisto again to use the new version.";
    }

    void OpenReleaseNotes()
    {
        noteRows.clear();
        nNotesScroll = 0;
        bNotesDrag = false;
        nNotesArrow = 0;
        sUpdateMessage.clear();

        const int nWidth = kNotesBox.w - 2 * kListPad - kScrollBarW - 12;
        const int nBulletW = TextW("-  ");
        std::vector<ltlaunch::NoteLine> lines = ltlaunch::ReleaseNotes(update.m_Release.m_sBody);
        if (lines.empty()) lines.push_back({ "This release has no notes.", false, false });

        for (const ltlaunch::NoteLine &line : lines)
        {
            const int nIndent = line.m_bBullet ? nBulletW : 0;
            std::string s = line.m_sText;
            bool bFirst = true;
            if (s.empty()) noteRows.push_back(NoteRow());

            // Wrapped at spaces, or anywhere in a word too long for a line
            while (!s.empty())
            {
                size_t n = s.size();
                while (n > 1 && TextW(s.substr(0, n)) > nWidth - nIndent)
                {
                    const size_t cut = s.find_last_of(' ', n - 1);
                    n = (cut == std::string::npos || cut == 0) ? n - 1 : cut;
                }
                NoteRow row;
                row.sText = s.substr(0, n);
                row.x = nIndent;
                row.bHeading = line.m_bHeading;
                row.bBullet = line.m_bBullet && bFirst;
                noteRows.push_back(row);
                bFirst = false;
                s.erase(0, n);
                while (!s.empty() && s[0] == ' ') s.erase(0, 1);
            }
        }
        eOpenDialog = DialogId::Release;
    }

    int NoteRowH() const { return g_nLineH + 2; }
    int VisibleNoteRows() const { return (kNotesBox.h - 2 * kListPad) / NoteRowH(); }

    int ClampNotesScroll(int nScroll) const
    {
        const int nMax = (int)noteRows.size() - VisibleNoteRows();
        if (nScroll > nMax) nScroll = nMax;
        return nScroll < 0 ? 0 : nScroll;
    }

    // The notes' scroll bar, empty when everything fits: an arrow at each end and the track between them
    Rect NotesBar() const
    {
        if ((int)noteRows.size() <= VisibleNoteRows()) return Rect();
        return Rect(kNotesBox.x + kNotesBox.w - kScrollBarW, kNotesBox.y + kListPad, kScrollBarW - kListPad,
                    kNotesBox.h - 2 * kListPad);
    }

    Rect NotesArrow(int nDir) const
    {
        const Rect bar = NotesBar();
        if (bar.h <= 0) return Rect();
        return Rect(bar.x, nDir < 0 ? bar.y : bar.y + bar.h - kScrollArrowH, bar.w, kScrollArrowH);
    }

    Rect NotesTrack() const
    {
        const Rect bar = NotesBar();
        if (bar.h <= 0) return Rect();
        return Rect(bar.x, bar.y + kScrollArrowH, bar.w, bar.h - 2 * kScrollArrowH);
    }

    Rect NotesHandle() const
    {
        const Rect track = NotesTrack();
        if (track.h <= 0) return Rect();
        const int nVis = VisibleNoteRows(), nRows = (int)noteRows.size();
        int nHandleH = track.h * nVis / nRows;
        if (nHandleH < kMinHandleH) nHandleH = kMinHandleH;
        return Rect(track.x, track.y + (track.h - nHandleH) * nNotesScroll / (nRows - nVis), track.w, nHandleH);
    }

    // A press on the bar. An arrow scrolls a line and repeats while held, the handle is dragged,
    // and the track either side of it pages up or down
    bool PressNotesBar(int x, int y)
    {
        if (eOpenDialog != DialogId::Release || !In(NotesBar(), x, y)) return false;
        for (int nDir = -1; nDir <= 1; nDir += 2)
            if (In(NotesArrow(nDir), x, y))
            {
                nNotesArrow = nDir;
                nNotesScroll = ClampNotesScroll(nNotesScroll + nDir);
                nArrowRepeatAt = SDL_GetTicks() + kArrowDelay;
                SDL_CaptureMouse(SDL_TRUE);
                return true;
            }

        const Rect handle = NotesHandle();
        if (In(handle, x, y))
        {
            bNotesDrag = true;
            nNotesGrabY = y - handle.y;
            SDL_CaptureMouse(SDL_TRUE); // Keeps the drag when the pointer leaves the window
        }
        else
            nNotesScroll = ClampNotesScroll(nNotesScroll + (y < handle.y ? -1 : 1) * (VisibleNoteRows() - 1));
        return true;
    }

    // Moves the handle with the pointer. True when the notes scrolled
    bool DragNotes(int y)
    {
        const Rect track = NotesTrack(), handle = NotesHandle();
        const int nTravel = track.h - handle.h;
        const int nMax = (int)noteRows.size() - VisibleNoteRows();
        if (nTravel <= 0 || nMax <= 0) return false;
        const int nTop = y - nNotesGrabY - track.y;
        const int nScroll = ClampNotesScroll((nTop * nMax + nTravel / 2) / nTravel);
        if (nScroll == nNotesScroll) return false;
        nNotesScroll = nScroll;
        return true;
    }

    bool NotesHeld() const { return bNotesDrag || nNotesArrow != 0; }

    void EndNotesDrag()
    {
        bNotesDrag = false;
        nNotesArrow = 0;
        SDL_CaptureMouse(SDL_FALSE);
    }

    // How long the event loop may wait before a held arrow scrolls again
    Uint32 ArrowWait() const
    {
        const Uint32 nNow = SDL_GetTicks();
        return nArrowRepeatAt > nNow ? nArrowRepeatAt - nNow : 0;
    }

    // A held arrow scrolls another line while the pointer is still on it. True when the notes scrolled
    bool RepeatArrow()
    {
        if (!nNotesArrow || SDL_GetTicks() < nArrowRepeatAt) return false;
        nArrowRepeatAt = SDL_GetTicks() + kArrowRepeat;
        int x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        if (!In(NotesArrow(nNotesArrow), x, y)) return false;
        const int nScroll = ClampNotesScroll(nNotesScroll + nNotesArrow);
        if (nScroll == nNotesScroll) return false;
        nNotesScroll = nScroll;
        return true;
    }

    // A scroll bar arrow, drawn as a small button with a triangle pointing up or down
    void DrawArrow(const Rect &r, int nDir, bool bPressed)
    {
        Fill(r, bPressed ? kButtonDown : kButton);
        Frame(r, bPressed ? kBorderDown : kBorder);
        const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        for (int i = 0; i < 4; ++i)
        {
            const int nRow = nDir < 0 ? cy - 2 + i : cy + 1 - i;
            Fill(Rect(cx - i, nRow, 2 * i + 1, 1), kText);
        }
    }

    // The hand over the version link
    void UpdateCursor(int x, int y, SDL_Cursor *pHand, SDL_Cursor *pArrow)
    {
        const bool bOver = eOpenDialog == DialogId::None && In(rUpdateLink, x, y);
        SDL_Cursor *pWant = bOver ? pHand : pArrow;
        if (pWant && SDL_GetCursor() != pWant) SDL_SetCursor(pWant);
    }
#endif

    void CloseDialog(bool bAccept)
    {
        if (eOpenDialog == DialogId::Display) CloseDisplay(bAccept);
        else if (eOpenDialog == DialogId::Customize) CloseCustomize(bAccept);
        eOpenDialog = DialogId::None;
    }

    // One list in a dialog
    struct ListRef
    {
        const Rect &rect;
        int nItems;
        int &nScroll;
        int &iSel;
    };

    // The open dialog's lists
    std::vector<ListRef> OpenLists()
    {
        if (eOpenDialog == DialogId::Display)
            return { { kSizeList, (int)sizes.size(), nSizeScroll, iSize } };
        if (eOpenDialog == DialogId::Customize)
            return { { kAvailableList, (int)availableRez.size(), nAvailableScroll, iAvailableSel },
                     { kChosenList, (int)chosenRez.size(), nChosenScroll, iChosenSel } };
        return {};
    }

    // Selects the row under a click
    static bool SelectRowAt(ListRef &list, int x, int y)
    {
        const int iRow = ListRowAt(list.rect, list.nItems, list.nScroll, x, y);
        if (iRow < 0) return false;
        if (iRow < list.nItems) list.iSel = iRow;
        return true;
    }

    bool ClickList(int x, int y)
    {
        for (ListRef &list : OpenLists())
            if (SelectRowAt(list, x, y)) return true;
        return false;
    }

    // Scrolls the list under the mouse
    bool ScrollList(int x, int y, int nRows)
    {
#ifdef LTLAUNCH_UPDATES
        if (eOpenDialog == DialogId::Release && In(kNotesBox, x, y))
        {
            nNotesScroll = ClampNotesScroll(nNotesScroll + nRows);
            return true;
        }
#endif
        for (ListRef &list : OpenLists())
            if (In(list.rect, x, y))
            {
                list.nScroll = ClampScroll(list.rect, list.nItems, list.nScroll + nRows);
                return true;
            }
        return false;
    }

    std::vector<std::string> SizeRows() const
    {
        std::vector<std::string> rows;
        for (size_t i = 0; i < sizes.size(); ++i) rows.push_back(ltlaunch::SizeText(sizes[i]));
        return rows;
    }

    std::string DisplayCfgPath() const { return ltlaunch::JoinPath(spec.m_sGameDir, "display.cfg"); }

    // Takes the size and borderless setting from display.cfg
    void ReadDisplayCfg()
    {
        const std::string sCfg = ltlaunch::ReadTextFile(DisplayCfgPath());
        std::string sWidth, sHeight, sBorderless;
        ltlaunch::WindowSize fileSize = { 0, 0 };
        if (ltlaunch::GetConfigValue(sCfg, "ScreenWidth", sWidth) &&
            ltlaunch::GetConfigValue(sCfg, "ScreenHeight", sHeight))
        {
            fileSize.m_nWidth = std::atoi(sWidth.c_str());
            fileSize.m_nHeight = std::atoi(sHeight.c_str());
            std::vector<ltlaunch::WindowSize> all = sizes;
            all.push_back(fileSize);
            sizes = ltlaunch::DistinctSizes(all);
        }

        iSize = -1;
        for (size_t i = 0; i < sizes.size(); ++i)
            if (sizes[i].m_nWidth == fileSize.m_nWidth && sizes[i].m_nHeight == fileSize.m_nHeight)
                iSize = (int)i;

        bBorderless = ltlaunch::GetConfigValue(sCfg, "BorderlessWindow", sBorderless) &&
                      std::atoi(sBorderless.c_str()) != 0;
    }

    void OpenDisplay()
    {
        ReadDisplayCfg();
        iSizeOpen = iSize;
        bBorderlessOpen = bBorderless;
        nSizeScroll = ClampScroll(kSizeList, (int)sizes.size(), iSize);
        eOpenDialog = DialogId::Display;
    }

    // OK writes what changed into display.cfg
    void CloseDisplay(bool bAccept)
    {
        if (!bAccept) { iSize = iSizeOpen; bBorderless = bBorderlessOpen; return; }

        std::vector<ltlaunch::ConfigValue> values;
        if (iSize != iSizeOpen && iSize >= 0)
        {
            values.push_back({ "ScreenWidth", std::to_string(sizes[(size_t)iSize].m_nWidth) });
            values.push_back({ "ScreenHeight", std::to_string(sizes[(size_t)iSize].m_nHeight) });
        }
        if (bBorderless != bBorderlessOpen)
            values.push_back({ "BorderlessWindow", bBorderless ? "1" : "0" });
        if (values.empty()) return;

        const std::string sCfg = ltlaunch::SetConfigValues(ltlaunch::ReadTextFile(DisplayCfgPath()), values);
        if (ltlaunch::WriteTextFile(DisplayCfgPath(), sCfg)) sStatus.clear();
        else sStatus = "Could not write " + DisplayCfgPath();
    }

    void OpenCustomize()
    {
        availableRez = ltlaunch::AvailableRez(spec);
        iAvailableSel = iChosenSel = -1;
        nAvailableScroll = nChosenScroll = 0;
        chosenRezOnOpen = chosenRez;
        bAlwaysRezOnOpen = bAlwaysRez;
        eOpenDialog = DialogId::Customize;
    }

    // OK saves the checkbox, and the chosen archives with it when it's ticked
    void CloseCustomize(bool bAccept)
    {
        if (!bAccept)
        {
            chosenRez = chosenRezOnOpen;
            bAlwaysRez = bAlwaysRezOnOpen;
            return;
        }

        SaveSettings();
    }

    void SaveSettings()
    {
        ltlaunch::LauncherSettings settings;
        settings.m_bAlwaysRez = bAlwaysRez;
        settings.m_AlwaysRez = chosenRez;
        settings.m_bAddOn = bAddOn;
        settings.m_sDataDir = spec.m_sDataDir;
        if (!ltlaunch::SaveSettings(sSettingsPath, settings)) sStatus = "Could not write " + sSettingsPath;
    }

    // A ticked checkbox from an earlier run brings its archives back
    void LoadSettings()
    {
        ltlaunch::LauncherSettings settings;
        if (!ltlaunch::LoadSettings(sSettingsPath, settings)) return;
        bAlwaysRez = settings.m_bAlwaysRez;
        if (bAlwaysRez) chosenRez = settings.m_AlwaysRez;
        bAddOn = settings.m_bAddOn;
        spec.m_sDataDir = settings.m_sDataDir;
    }

    // A standard install's folder to read the archives from. The game folder goes back to the default
    void BrowseDataDir()
    {
        std::string sDir, sError;
        if (!PickFolder(pWnd, ltlaunch::DataDir(spec), sDir, sError))
        {
            if (!sError.empty()) sStatus = sError;
            return;
        }

        spec.m_sDataDir = ltlaunch::SameDir(sDir, spec.m_sGameDir) ? std::string() : sDir;
        bSplashTried = false;
        sStatus.clear();
        SaveSettings();
    }

    // The addon has its own banner, so the splash is loaded again
    // Shows or hides the games list. The window narrows or widens on its left, so the rest stays where it is
    void ToggleGameList()
    {
        const bool bShow = g_nSideW == 0;
        Layout(bShow);
        if (pWnd)
        {
            int x = 0, y = 0;
            SDL_GetWindowPosition(pWnd, &x, &y);
            SDL_SetWindowSize(pWnd, g_nWinW, kWinH);
            SDL_SetWindowPosition(pWnd, bShow ? x - kGameListW : x + kGameListW, y);
        }
        if (!sSelectedPath.empty()) ltlaunch::SetIniValue(sSelectedPath, "gamelist", bShow ? "1" : "0");
    }

    // A small button holding a double arrow pointing the way the games list will move
    void DrawGameListToggle()
    {
        const Rect &r = kGameListToggle;
        const bool bPressed = Down(r);
        Fill(r, bPressed ? kButtonDown : kButton);
        Frame(r, bPressed ? kBorderDown : kBorder);
        const bool bLeft = g_nSideW == 0;
        const int cy = r.y + r.h / 2 + (bPressed ? 1 : 0);
        const int x0 = r.x + (r.w - 8) / 2 + (bPressed ? 1 : 0);
        for (int n = 0; n < 2; ++n)
            for (int i = 0; i < 4; ++i)
            {
                const int nCol = bLeft ? x0 + n * 4 + i : x0 + n * 4 + 3 - i;
                Fill(Rect(nCol, cy - i, 1, 2 * i + 1), kText);
            }
    }

    void ToggleAddOn()
    {
        bAddOn = !bAddOn;
        bSplashTried = false;
        SaveSettings();
    }

    void AddRez()
    {
        if (iAvailableSel < 0 || iAvailableSel >= (int)availableRez.size()) return;
        const std::string &sRez = availableRez[(size_t)iAvailableSel];
        if (!ltlaunch::ContainsNoCase(chosenRez, sRez)) chosenRez.push_back(sRez);
    }

    void RemoveRez()
    {
        if (iChosenSel < 0 || iChosenSel >= (int)chosenRez.size()) return;
        chosenRez.erase(chosenRez.begin() + iChosenSel);

        if (iChosenSel >= (int)chosenRez.size()) iChosenSel = (int)chosenRez.size() - 1;
        nChosenScroll = ClampScroll(kChosenList, (int)chosenRez.size(), nChosenScroll);
    }

    void Draw()
    {
        Fill(Rect(0, 0, g_nWinW, kWinH), kFace);

        // The folder the archives are read from
        Fill(kDataDirBox, kWindow);
        Frame(kDataDirBox, kBorder);
        Text(kDataDirBox.x + 4, kDataDirBox.y + (kDataDirBox.h - g_nLineH) / 2,
             FitLeft(ltlaunch::DataDir(spec), kDataDirBox.w - 8), kText, kDataDirBox.w - 8);
        Button(kBrowseButton, "Browse...", Down(kBrowseButton));
        if (kToggleW > 0) DrawGameListToggle();

        if (g_nSideW > 0) DrawGameList();

        const Rect &panel = kBannerPanel;
        Fill(panel, kWindow);
        Frame(panel, kBorder);

        const std::vector<std::string> required = Required();
        std::vector<bool> present;
        bool bMissing = false;
        for (const std::string &sRez : required)
        {
            present.push_back(HasData(sRez));
            bMissing = bMissing || !present.back();
        }
        if (bMissing)
        {
            DrawDataMissing(panel, required, present);
            bSplashTried = false; // loaded again once the archives are back
        }
        else
            DrawSplash(panel);

        // An addon without a label of its own launches under the plain one
        const bool bAddOnLabel = UseAddOn() && !game.m_AddOn.m_sButton.empty();
        const std::string sLaunch = game.m_sLaunch.empty() ? std::string("Launch") : game.m_sLaunch;
        Button(kPlayButton, bAddOnLabel ? game.m_AddOn.m_sButton : sLaunch, Down(kPlayButton));
        Button(kDisplayButton, "Display...", Down(kDisplayButton));
        Button(kCustomizeButton, "Customize...", Down(kCustomizeButton));
        Button(kQuitButton, "Quit", Down(kQuitButton));
        if (bHasAddOn) CheckBox(kAddOnBox, game.m_AddOn.m_sLabel, bAddOn, Down(kAddOnBox));

        // A refused or failed launch
        Text(g_nSideW + 12, kStatusY, sStatus, kAlert, kMainW - 24);

#ifdef LTLAUNCH_UPDATES
        DrawUpdateNotice();
#endif

        // The footer
        const std::string sLine = std::string(kLauncherName) + " v" + g_szLauncherBuild;
        Text((g_nWinW - TextW(sLine)) / 2, kFooterY, sLine, kText);

        if (eOpenDialog == DialogId::Display) DrawDisplayDialog();
        else if (eOpenDialog == DialogId::Customize) DrawCustomizeDialog();
#ifdef LTLAUNCH_UPDATES
        else if (eOpenDialog == DialogId::Release) DrawReleaseDialog();
#endif
    }

#ifdef LTLAUNCH_UPDATES
    int DownloadPercent() const
    {
        if (!update.m_nTotal) return 0;
        const int n = (int)(update.m_nDone * 100 / update.m_nTotal);
        return n > 100 ? 100 : n;
    }

    // One line naming the newer release, its version a link to the notes, which hold the buttons
    void DrawUpdateNotice()
    {
        update = pUpdater ? pUpdater->Get() : ltlaunch::Updater::View();
        rUpdateLink = Rect();
        if (update.m_eState == ltlaunch::Updater::State::None) return;

        std::string sStamp = update.m_Release.m_sTag;
        if (!sStamp.empty() && (sStamp[0] == 'v' || sStamp[0] == 'V')) sStamp.erase(0, 1);
        const std::string sVersion = std::string(kLauncherName) + " v" + sStamp;

        std::string sBefore, sAfter;
        switch (update.m_eState)
        {
            case ltlaunch::Updater::State::Available:   sBefore = "Update available: "; break;
            case ltlaunch::Updater::State::Downloading:
                sBefore = "Downloading ";
                sAfter = "  " + std::to_string(DownloadPercent()) + "%";
                break;
            case ltlaunch::Updater::State::Ready:       sAfter = " is ready to install"; break;
            default:                                    sBefore = "Update failed: "; break;
        }

        // Centered under the footer, but kept clear of the game list
        const int nLinkW = TextW(sVersion);
        int x = (g_nWinW - TextW(sBefore) - nLinkW - TextW(sAfter)) / 2;
        if (x < g_nSideW + 12) x = g_nSideW + 12;
        const int y = kUpdateY;
        const Uint32 cText = update.m_eState == ltlaunch::Updater::State::Failed ? kAlert : kText;

        Text(x, y, sBefore, cText);
        x += TextW(sBefore);
        Text(x, y, sVersion, kLink);
        Fill(Rect(x, y + g_nAscent + 2, nLinkW, 1), kLink);
        rUpdateLink = Rect(x, y - 2, nLinkW, g_nLineH + 4);
        Text(x + nLinkW, y, sAfter, cText);
    }

    // The line above the release notes' buttons, centered: the download's progress, what's next, or what went wrong
    void DrawUpdateStatus()
    {
        const Rect line(kNotesBox.x, kNotesBox.y + kNotesBox.h + 2, kNotesBox.w, kUpdateStatusH);
        const int y = line.y + (line.h - g_nLineH) / 2;

        if (!sUpdateMessage.empty())
        {
            TextC(line, sUpdateMessage, kAlert);
            return;
        }

        switch (update.m_eState)
        {
            case ltlaunch::Updater::State::Available:
                TextC(line, "", kText);
                break;
            case ltlaunch::Updater::State::Downloading:
            {
                // The percentage and its bar, centered together
                const std::string sText = "Downloading...  " + std::to_string(DownloadPercent()) + "%";
                const int nBarW = 240, nGap = 12;
                const int x = line.x + (line.w - TextW(sText) - nGap - nBarW) / 2;
                Text(x, y, sText, kText);
                const Rect bar(x + TextW(sText) + nGap, y + (g_nLineH - 10) / 2, nBarW, 10);
                Fill(bar, kWindow);
                Frame(bar, kBorder);
                const int nFill = (bar.w - 2) * DownloadPercent() / 100;
                Fill(Rect(bar.x + 1, bar.y + 1, nFill, bar.h - 2), kCaption);
                break;
            }
            case ltlaunch::Updater::State::Ready:
                TextC(line, "Downloaded! Restart LTCallisto to finish updating.", kText);
                break;
            case ltlaunch::Updater::State::Failed:
                TextC(line, "Update failed: " + update.m_sError, kAlert);
                break;
            default:
                break;
        }
    }

    void DrawReleaseDialog()
    {
        std::string sTitle = ltlaunch::ToAscii(update.m_Release.m_sName);
        if (sTitle.empty()) sTitle = std::string(kLauncherName) + " " + update.m_Release.m_sTag;
        DrawDialog(kReleaseDlg, sTitle, UpdateLabel(), "Close");
        Button(kGitHubButton, "View on GitHub", Down(kGitHubButton));
        DrawUpdateStatus();

        Fill(kNotesBox, kWindow);
        Frame(kNotesBox, kBorder);
        const int nVis = VisibleNoteRows();
        const int nRows = (int)noteRows.size();
        const int nBulletW = TextW("-  ");
        for (int n = 0; n < nVis && nNotesScroll + n < nRows; ++n)
        {
            const NoteRow &row = noteRows[(size_t)(nNotesScroll + n)];
            const int x = kNotesBox.x + kListPad + 6 + row.x;
            const int y = kNotesBox.y + kListPad + n * NoteRowH() + 1;
            if (row.bBullet) Text(x - nBulletW, y, "-", kText);
            Text(x, y, row.sText, kText);
            if (row.bHeading) Text(x + 1, y, row.sText, kText);
        }

        if (nRows > nVis)
        {
            Fill(NotesBar(), kFace);
            DrawArrow(NotesArrow(-1), -1, nNotesArrow < 0);
            DrawArrow(NotesArrow(1), 1, nNotesArrow > 0);
            const Rect handle = NotesHandle();
            Fill(handle, bNotesDrag ? kButtonDown : kButton);
            Frame(handle, bNotesDrag ? kBorderDown : kBorder);
        }
    }
#endif

    // The combined launcher's games, the shown one drawn as held down, then a rule between the list and the game
    void DrawGameList()
    {
        const std::string sHead = "Games";
        const int hx = kGamesHeading.x + (kGamesHeading.w - TextW(sHead)) / 2;
        const int hy = kGamesHeading.y + (kGamesHeading.h - g_nLineH) / 2;
        Text(hx, hy, sHead, kText);
        Fill(Rect(hx, hy + g_nLineH, TextW(sHead), 1), kText);

        for (size_t i = 0; i < gameButtons.size(); ++i)
        {
            const Rect &r = gameButtons[i];
            if ((int)i == iGame && !Down(r))
            {
                Fill(r, kButtonDown);
                Frame(r, kBorderDown);
                TextC(r, games[i].m_sName, kText);
            }
            else
                Button(r, games[i].m_sName, Down(r));
        }

        Fill(Rect(g_nSideW - 1, 12, 1, kWinH - 24), kBorder);
    }

    void LoadSplash()
    {
        bSplashTried = true;
        if (pSplash) { SDL_DestroyTexture(pSplash); pSplash = 0; }

        // The addon's banner, or the game's when it has none
        std::vector<unsigned char> raw;
        ltlaunch::Image img;
        const bool bAddOnSplash = UseAddOn() &&
            ltlaunch::ReadRezEntry(ltlaunch::JoinPath(ltlaunch::DataDir(spec), game.m_AddOn.m_sSplashRez),
                                   game.m_AddOn.m_sSplashPath, raw) &&
            ltlaunch::DecodePcx(raw, img);
        if (!bAddOnSplash &&
            (!ltlaunch::ReadRezEntry(ltlaunch::JoinPath(ltlaunch::DataDir(spec), game.m_sSplashRez), game.m_sSplashPath, raw) ||
             !ltlaunch::DecodePcx(raw, img)))
            return;

        pSplash = SDL_CreateTexture(g_pRen, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
                                    img.m_nWidth, img.m_nHeight);
        if (!pSplash) return;
        SDL_UpdateTexture(pSplash, 0, &img.m_RGB[0], img.m_nWidth * 3);
        SDL_SetTextureScaleMode(pSplash, SDL_ScaleModeLinear);
    }

    void DrawSplash(const Rect &panel)
    {
        if (!bSplashTried) LoadSplash();
        if (!pSplash) return;

        int nW = 0, nH = 0;
        SDL_QueryTexture(pSplash, 0, 0, &nW, &nH);
        const Rect in(panel.x + 1, panel.y + 1, panel.w - 2, panel.h - 2);
        int w = in.w, h = nH * in.w / nW;
        if (h > in.h) { h = in.h; w = nW * in.h / nH; }
        Fill(in, 0x000000);
        SDL_Rect dst = { in.x + (in.w - w) / 2, in.y + (in.h - h) / 2, w, h };
        SDL_RenderCopy(g_pRen, pSplash, 0, &dst);
    }

    // Names each archive the game needs, whether it was found, and the folder they belong in
    void DrawDataMissing(const Rect &panel, const std::vector<std::string> &required, const std::vector<bool> &present)
    {
        const Rect in(panel.x + 3, panel.y + 3, panel.w - 6, panel.h - 6);
        const int nMaxW = in.w - 8;
        int y = in.y + 8;

        // Wrapped at spaces to the panel
        auto Wrapped = [&](std::string s)
        {
            while (!s.empty())
            {
                size_t n = s.size();
                while (n > 0 && TextW(s.substr(0, n)) > nMaxW)
                {
                    const size_t cut = s.find_last_of(' ', n - 1);
                    n = (cut == std::string::npos || cut == 0) ? n - 1 : cut;
                }
                if (n == 0) n = 1;
                Text(in.x + 4, y, s.substr(0, n), kText, nMaxW);
                s.erase(0, n);
                while (!s.empty() && s[0] == ' ') s.erase(0, 1);
                y += g_nLineH;
            }
        };

        const std::string sHead = "GAME DATA MISSING";
        const int hx = in.x + (in.w - TextW(sHead)) / 2;
        Text(hx, y, sHead, kAlert);
        Text(hx + 1, y, sHead, kAlert);
        y += g_nLineH + 6;

        Wrapped(spec.m_sDataDir.empty() ? "This game's archives were not found. Copy them into:"
                                        : "This game's archives were not found in:");
        y += 4;

        std::string sDir = ltlaunch::DataDir(spec);
        while (!sDir.empty())
        {
            size_t n = 1;
            while (n < sDir.size() && TextW(sDir.substr(0, n + 1)) <= nMaxW) ++n;
            if (n < sDir.size())
            {
                const size_t cut = sDir.find_last_of("\\/", n - 1);
                if (cut != std::string::npos && cut > 0) n = cut + 1;
            }
            Text(in.x + 4, y, sDir.substr(0, n), kText, nMaxW);
            sDir.erase(0, n);
            y += g_nLineH;
        }
        y += 6;

        Wrapped(spec.m_sDataDir.empty() ? "Or press Browse... to point to your copy of the game."
                                        : "Press Browse... to point to your copy of the game.");
        y += 6;

        // In two columns when a long folder name leaves too little room for one
        const int nRoom = (in.y + in.h - y) / g_nLineH;
        const int nCols = (int)required.size() > nRoom ? 2 : 1;
        const int nPerCol = ((int)required.size() + nCols - 1) / nCols;
        const int nColW = (in.w - 8) / nCols;
        for (size_t i = 0; i < required.size(); ++i)
        {
            const int x = in.x + 4 + (int)i / nPerCol * nColW;
            const int ry = y + (int)i % nPerCol * g_nLineH;
            if (ry + g_nLineH > in.y + in.h) continue;
            Text(x + 8, ry, present[i] ? "found" : "missing", present[i] ? kGrayTxt : kAlert);
            Text(x + 72, ry, required[i], present[i] ? kGrayTxt : kText, nColW - 76);
        }
    }

    // The frame, caption bar, close box, OK and Cancel every dialog shares. An empty label leaves its button out
    void DrawDialog(const Dialog &d, const std::string &sTitle, const std::string &sOk = "OK",
                    const std::string &sCancel = "Cancel")
    {
        Fill(d.frame, kFace);
        Frame(d.frame, kBorder);
        Fill(d.caption, kCaption);
        Text(d.caption.x + 6, d.caption.y + (d.caption.h - g_nLineH) / 2, sTitle, kCapText, d.caption.w - 32);
        Button(d.close, "x", Down(d.close));
        if (!sOk.empty()) Button(d.ok, sOk, Down(d.ok));
        if (!sCancel.empty()) Button(d.cancel, sCancel, Down(d.cancel));
    }

    void DrawDisplayDialog()
    {
        DrawDialog(kDisplayDlg, "Display Settings");
        Text(kSizeList.x, kSizeList.y - 18, "Window size:", kText);
        ListBox(kSizeList, SizeRows(), iSize, nSizeScroll);
        CheckBox(kBorderlessBox, "Borderless window", bBorderless, Down(kBorderlessBox));
    }

    void DrawCustomizeDialog()
    {
        DrawDialog(kCustomizeDlg, "Customize " + game.m_sTitle);
        Text(kAvailableList.x, kAvailableList.y - 16, "Available rez files:", kText);
        ListBox(kAvailableList, availableRez, iAvailableSel, nAvailableScroll);
        Button(kAddButton, "Add >", Down(kAddButton));
        Button(kRemoveButton, "< Remove", Down(kRemoveButton));
        Text(kChosenList.x, kChosenList.y - 16, "Rez files to load:", kText);
        ListBox(kChosenList, chosenRez, iChosenSel, nChosenScroll);
        CheckBox(kAlwaysRezBox, "Always load these rez files", bAlwaysRez, Down(kAlwaysRezBox));
    }
};

} // namespace

int LauncherMain()
{
#ifdef _WIN32
    SDL_SetMainReady();
#endif

    // A click that activates the window still reaches the buttons
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

#if defined(_WIN32) && defined(LTLAUNCH_COMBINED)
    // The window and taskbar take the exe's own icon, resource 101 in ltcallisto.rc
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "101");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "101");
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    // Lithtech.exe and the game directory are found from where the launcher runs
    char *pBasePath = SDL_GetBasePath();
    const std::string sExeDir = pBasePath ? pBasePath : ".";
    if (pBasePath) SDL_free(pBasePath);

    App app;
    int iStart = 0;
#ifdef LTLAUNCH_COMBINED
    app.games = ltlaunch::CombinedProfiles(sExeDir);
    app.sSelectedPath = ltlaunch::JoinPath(sExeDir, "launcher.ini");
    const std::string sSelected = ltlaunch::LoadSelectedGame(app.sSelectedPath);
    for (size_t i = 0; i < app.games.size(); ++i)
        if (app.games[i].m_sKey == sSelected) iStart = (int)i;
    const std::string sCaption = kLauncherName;
#else
    app.games.push_back(ltlaunch::LTLAUNCH_PROFILE(sExeDir));
    const std::string sCaption = app.games[0].m_sCaption;
#endif
    app.states.resize(app.games.size());
    if (kGameListW > 0)
        for (size_t i = 0; i < app.games.size(); ++i)
            app.gameButtons.push_back(Rect(12, kGameButtonY + (int)i * kGameButtonStep, kGameListW - 24, 26));
    app.SelectGame(iStart);

    // The games list is shown unless it was hidden last time (gamelist=0 in launcher.ini)
    Layout(kGameListW > 0 && ltlaunch::GetIniValue(app.sSelectedPath, "gamelist") != "0");

    app.sizes = DesktopSizes();

    SDL_Window *pWnd = SDL_CreateWindow(sCaption.c_str(),
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        g_nWinW, kWinH, SDL_WINDOW_SHOWN);
    if (!pWnd) { SDL_Quit(); return 1; }
    app.pWnd = pWnd;
    g_pRen = SDL_CreateRenderer(pWnd, -1, SDL_RENDERER_SOFTWARE);
    if (!g_pRen) { SDL_DestroyWindow(pWnd); SDL_Quit(); return 1; }
    g_pFont = MakeFontAtlas();
    if (!g_pFont)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, sCaption.c_str(),
                                 "Could not load the Tahoma font.", pWnd);
        SDL_DestroyRenderer(g_pRen);
        SDL_DestroyWindow(pWnd);
        SDL_Quit();
        return 1;
    }

#ifdef LTLAUNCH_UPDATES
    // Checks GitHub for a newer build. Off when launcher.ini says updates=0, and in a build's output folder,
    // where an update would replace what was just built. updatebuild=<stamp> stands in for this build's stamp
    const Uint32 nUpdateEvent = SDL_RegisterEvents(1);
    SDL_Cursor *pHand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    SDL_Cursor *pArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    app.sExeDir = sExeDir;
    if (ltlaunch::GetIniValue(app.sSelectedPath, "updates") != "0" && !ltlaunch::InBuildTree(sExeDir) &&
        nUpdateEvent != (Uint32)-1)
    {
        std::string sBuild = ltlaunch::GetIniValue(app.sSelectedPath, "updatebuild");
        if (sBuild.empty()) sBuild = g_szLauncherBuild;
        app.pUpdater.reset(new ltlaunch::Updater(LTCALLISTO_UPDATE_REPO, sExeDir, sBuild, [nUpdateEvent]() {
            SDL_Event ev;
            SDL_zero(ev);
            ev.type = nUpdateEvent;
            SDL_PushEvent(&ev);
        }));
        app.pUpdater->StartCheck();
    }
#endif

    bool bRedraw = true;
    while (!app.bQuit)
    {
        if (bRedraw)
        {
            app.Draw();
            SDL_RenderPresent(g_pRen);
            bRedraw = false;
        }

        SDL_Event e;
#ifdef LTLAUNCH_UPDATES
        // A held scroll bar arrow repeats, so the wait ends in time for its next line
        if (app.nNotesArrow)
        {
            if (!SDL_WaitEventTimeout(&e, (int)app.ArrowWait() + 1))
            {
                if (app.RepeatArrow()) bRedraw = true;
                continue;
            }
        }
        else
#endif
        if (!SDL_WaitEvent(&e)) break;
        do
        {
            if (e.type == SDL_QUIT)
                app.bQuit = true;
#ifdef LTLAUNCH_UPDATES
            else if (e.type == nUpdateEvent)
                bRedraw = true;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT &&
                     app.PressNotesBar(e.button.x, e.button.y))
                bRedraw = true;
            else if (e.type == SDL_MOUSEMOTION && app.bNotesDrag)
            {
                if (app.DragNotes(e.motion.y)) bRedraw = true;
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && app.NotesHeld())
            {
                app.EndNotesDrag();
                bRedraw = true;
            }
            else if (e.type == SDL_MOUSEMOTION && !app.pPressed)
                app.UpdateCursor(e.motion.x, e.motion.y, pHand, pArrow);
#endif
            else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_EXPOSED ||
                                                   e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED))
                bRedraw = true;
            else if (e.type == SDL_KEYDOWN && app.eOpenDialog != App::DialogId::None &&
                     (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_RETURN ||
                      e.key.keysym.sym == SDLK_KP_ENTER))
            {
                const bool bAccept = e.key.keysym.sym != SDLK_ESCAPE;
                app.CloseDialog(bAccept);
                app.pPressed = 0;
                bRedraw = true;
            }
            else if (e.type == SDL_MOUSEWHEEL)
            {
                int mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                if (app.ScrollList(mx, my, -e.wheel.y * 3)) bRedraw = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                if (app.ClickList(e.button.x, e.button.y))
                    bRedraw = true;
                else
                {
                    app.pPressed = app.ButtonAt(e.button.x, e.button.y);
                    app.bPressedOver = true;
                    if (app.pPressed) bRedraw = true;
                }
            }
            else if (e.type == SDL_MOUSEMOTION && app.pPressed)
            {
                const bool bOver = In(*app.pPressed, e.motion.x, e.motion.y);
                if (bOver != app.bPressedOver) { app.bPressedOver = bOver; bRedraw = true; }
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && app.pPressed)
            {
                const Rect *p = app.pPressed;
                app.pPressed = 0;
                if (In(*p, e.button.x, e.button.y)) app.Press(p);
                bRedraw = true;
            }
        } while (SDL_PollEvent(&e));

#ifdef LTLAUNCH_UPDATES
        // Events arriving steadily, like a moving mouse, mustn't hold up a held arrow
        if (app.nNotesArrow && app.RepeatArrow()) bRedraw = true;
#endif
    }

#ifdef LTLAUNCH_UPDATES
    // The window goes first, so waiting for a download to stop isn't seen
    SDL_HideWindow(pWnd);
    app.pUpdater.reset();
    for (ProcessRef p : app.gameProcesses) ReleaseProcess(p);
    if (pHand) SDL_FreeCursor(pHand);
    if (pArrow) SDL_FreeCursor(pArrow);
#endif

    if (app.pSplash) SDL_DestroyTexture(app.pSplash);
    if (g_pFont) SDL_DestroyTexture(g_pFont);
    SDL_DestroyRenderer(g_pRen);
    SDL_DestroyWindow(pWnd);
    SDL_Quit();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return LauncherMain();
}
#else
int main()
{
    return LauncherMain();
}
#endif
