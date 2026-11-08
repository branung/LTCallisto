#include "launcher_http.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace ltlaunch {

namespace {

std::wstring g_sAgent = L"LTCallisto-Launcher";

std::wstring Widen(const std::string &s)
{
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// Closes a WinHTTP handle when it goes out of scope
struct Handle
{
    HINTERNET h = NULL;
    explicit Handle(HINTERNET p) : h(p) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};

// One GET, handing each block received to sink. Redirects are followed
bool Get(const std::string &sUrl, const std::string &sAccept,
         const std::function<bool(const char *, DWORD, uint64_t)> &sink, std::string &sError)
{
    const std::wstring wUrl = Widen(sUrl);
    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (sUrl.compare(0, 8, "https://") != 0 || !WinHttpCrackUrl(wUrl.c_str(), 0, 0, &uc))
    {
        sError = "Bad address " + sUrl;
        return false;
    }
    const std::wstring wHost(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring wPath(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo) wPath.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    Handle session(WinHttpOpen(g_sAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.h) // Before Windows 8.1
        session.h = WinHttpOpen(g_sAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.h) { sError = "Could not start a connection"; return false; }
    WinHttpSetTimeouts(session.h, 10000, 10000, 15000, 20000);

    Handle connect(WinHttpConnect(session.h, wHost.c_str(), uc.nPort, 0));
    Handle request(connect.h ? WinHttpOpenRequest(connect.h, L"GET", wPath.c_str(), NULL, WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                             : NULL);
    if (!request.h) { sError = "Could not reach GitHub"; return false; }

    const std::wstring wHeaders = L"Accept: " + Widen(sAccept) + L"\r\n";
    if (!WinHttpSendRequest(request.h, wHeaders.c_str(), (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, NULL))
    {
        sError = "Could not reach GitHub";
        return false;
    }

    DWORD dwStatus = 0, dwSize = sizeof(dwStatus);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatus, &dwSize, WINHTTP_NO_HEADER_INDEX);
    if (dwStatus != 200)
    {
        if (dwStatus == 403 || dwStatus == 429) sError = "GitHub is limiting requests. Try again later.";
        else if (dwStatus == 404) sError = "GitHub has no release to offer.";
        else sError = "GitHub answered " + std::to_string(dwStatus);
        return false;
    }

    uint64_t nTotal = 0;
    wchar_t wLength[32] = { 0 };
    dwSize = sizeof(wLength);
    if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                            wLength, &dwSize, WINHTTP_NO_HEADER_INDEX))
        nTotal = _wcstoui64(wLength, NULL, 10);

    std::vector<char> buf(64 * 1024);
    for (;;)
    {
        DWORD dwRead = 0;
        if (!WinHttpReadData(request.h, &buf[0], (DWORD)buf.size(), &dwRead))
        {
            sError = "The connection to GitHub was lost";
            return false;
        }
        if (dwRead == 0) break;
        if (!sink(&buf[0], dwRead, nTotal)) { if (sError.empty()) sError = "Stopped"; return false; }
    }
    return true;
}

} // namespace

void SetHttpUserAgent(const std::string &sAgent)
{
    g_sAgent = Widen(sAgent);
}

bool HttpGet(const std::string &sUrl, const std::string &sAccept, std::string &sOut, size_t nMax,
             std::string &sError)
{
    sOut.clear();
    return Get(sUrl, sAccept, [&](const char *p, DWORD n, uint64_t) {
        if (sOut.size() + n > nMax) { sError = "GitHub's answer was too long"; return false; }
        sOut.append(p, n);
        return true;
    }, sError);
}

bool HttpDownload(const std::string &sUrl, const std::string &sPath, const HttpProgress &progress,
                  std::string &sError)
{
    std::ofstream out(sPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) { sError = "Could not save the download in the launcher's folder"; return false; }
    uint64_t nDone = 0;
    const bool bOk = Get(sUrl, "application/octet-stream", [&](const char *p, DWORD n, uint64_t nTotal) {
        out.write(p, n);
        if (!out) { sError = "Could not save the download in the launcher's folder"; return false; }
        nDone += n;
        return !progress || progress(nDone, nTotal);
    }, sError);
    out.close();
    if (bOk && !out) { sError = "Could not save the download in the launcher's folder"; return false; }
    return bOk;
}

bool Sha256File(const std::string &sPath, std::string &sHex, std::string &sError)
{
    sError = "Could not check the download";
    std::ifstream in(sPath.c_str(), std::ios::binary);
    if (!in) return false;

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return false;
    bool bOk = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) == 0;

    std::vector<char> buf(64 * 1024);
    while (bOk && in)
    {
        in.read(&buf[0], (std::streamsize)buf.size());
        const std::streamsize n = in.gcount();
        if (n > 0) bOk = BCryptHashData(hHash, (PUCHAR)&buf[0], (ULONG)n, 0) == 0;
    }

    unsigned char digest[32];
    if (bOk) bOk = BCryptFinishHash(hHash, digest, sizeof(digest), 0) == 0;
    if (hHash) BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!bOk) return false;

    char sByte[3];
    sHex.clear();
    for (unsigned char b : digest)
    {
        std::snprintf(sByte, sizeof(sByte), "%02x", b);
        sHex += sByte;
    }
    sError.clear();
    return true;
}

bool OpenInBrowser(const std::string &sUrl)
{
    if (sUrl.compare(0, 19, "https://github.com/") != 0) return false;
    for (char c : sUrl)
        if ((unsigned char)c <= ' ' || c == '"') return false;
    return (INT_PTR)ShellExecuteA(NULL, "open", sUrl.c_str(), NULL, NULL, SW_SHOWNORMAL) > 32;
}

} // namespace ltlaunch
