// The updater's network calls on Linux: libcurl, and xdg-open for the browser.
// libcurl is loaded when first needed rather than linked, so the launcher starts on any distribution,
// and only the update check is off where libcurl is missing. Linking it would also tie the launcher
// to the build machine's libcurl flavor, whose versioned symbols other distributions don't have
#include "launcher_http.h"

#include <curl/curl.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <mutex>

namespace ltlaunch {

namespace {

std::string g_sAgent = "LTCallisto-Launcher";

// The libcurl calls the updater makes, found in the system's library
struct Curl
{
    bool bLoaded = false;
    decltype(&curl_easy_init) easy_init = nullptr;
    decltype(&curl_easy_setopt) easy_setopt = nullptr;
    decltype(&curl_easy_perform) easy_perform = nullptr;
    decltype(&curl_easy_getinfo) easy_getinfo = nullptr;
    decltype(&curl_easy_cleanup) easy_cleanup = nullptr;
    decltype(&curl_slist_append) slist_append = nullptr;
    decltype(&curl_slist_free_all) slist_free_all = nullptr;
    decltype(&curl_global_init) global_init = nullptr;
};

const Curl *LoadCurl()
{
    static Curl curl;
    static std::once_flag once;
    std::call_once(once, []() {
        // The usual name, then Debian's and Ubuntu's GnuTLS build
        void *h = nullptr;
        for (const char *pName : { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl.so" })
            if ((h = dlopen(pName, RTLD_NOW | RTLD_LOCAL)) != nullptr) break;
        if (!h) return;
        curl.easy_init = (decltype(curl.easy_init))dlsym(h, "curl_easy_init");
        curl.easy_setopt = (decltype(curl.easy_setopt))dlsym(h, "curl_easy_setopt");
        curl.easy_perform = (decltype(curl.easy_perform))dlsym(h, "curl_easy_perform");
        curl.easy_getinfo = (decltype(curl.easy_getinfo))dlsym(h, "curl_easy_getinfo");
        curl.easy_cleanup = (decltype(curl.easy_cleanup))dlsym(h, "curl_easy_cleanup");
        curl.slist_append = (decltype(curl.slist_append))dlsym(h, "curl_slist_append");
        curl.slist_free_all = (decltype(curl.slist_free_all))dlsym(h, "curl_slist_free_all");
        curl.global_init = (decltype(curl.global_init))dlsym(h, "curl_global_init");
        curl.bLoaded = curl.easy_init && curl.easy_setopt && curl.easy_perform && curl.easy_getinfo &&
                       curl.easy_cleanup && curl.slist_append && curl.slist_free_all && curl.global_init;
        // Once, before any thread uses it, as libcurl asks
        if (curl.bLoaded) curl.global_init(CURL_GLOBAL_DEFAULT);
    });
    return curl.bLoaded ? &curl : nullptr;
}

struct Transfer
{
    const std::function<bool(const char *, size_t, uint64_t)> *pSink;
    uint64_t nTotal = 0;
    bool bStopped = false;
};

size_t OnData(char *p, size_t nSize, size_t nCount, void *pUser)
{
    Transfer *t = (Transfer *)pUser;
    const size_t n = nSize * nCount;
    if (!(*t->pSink)(p, n, t->nTotal)) { t->bStopped = true; return 0; }
    return n;
}

int OnProgress(void *pUser, curl_off_t nTotal, curl_off_t, curl_off_t, curl_off_t)
{
    if (nTotal > 0) ((Transfer *)pUser)->nTotal = (uint64_t)nTotal;
    return 0;
}

// One GET, handing each block received to sink. Redirects are followed, over HTTPS only
bool Get(const std::string &sUrl, const std::string &sAccept,
         const std::function<bool(const char *, size_t, uint64_t)> &sink, std::string &sError)
{
    if (sUrl.compare(0, 8, "https://") != 0) { sError = "Bad address " + sUrl; return false; }
    const Curl *c = LoadCurl();
    if (!c) { sError = "Updates need libcurl, which this system doesn't have"; return false; }

    CURL *h = c->easy_init();
    if (!h) { sError = "Could not start a connection"; return false; }
    curl_slist *pHeaders = c->slist_append(nullptr, ("Accept: " + sAccept).c_str());
    Transfer t;
    t.pSink = &sink;

    c->easy_setopt(h, CURLOPT_URL, sUrl.c_str());
    c->easy_setopt(h, CURLOPT_USERAGENT, g_sAgent.c_str());
    c->easy_setopt(h, CURLOPT_HTTPHEADER, pHeaders);
    c->easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
    c->easy_setopt(h, CURLOPT_PROTOCOLS_STR, "https");
    c->easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    c->easy_setopt(h, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    c->easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
#endif
    c->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
    // Gives up when the transfer stalls, as WinHTTP's receive timeout does
    c->easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
    c->easy_setopt(h, CURLOPT_LOW_SPEED_TIME, 20L);
    c->easy_setopt(h, CURLOPT_WRITEFUNCTION, &OnData);
    c->easy_setopt(h, CURLOPT_WRITEDATA, &t);
    c->easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
    c->easy_setopt(h, CURLOPT_XFERINFOFUNCTION, &OnProgress);
    c->easy_setopt(h, CURLOPT_XFERINFODATA, &t);
    c->easy_setopt(h, CURLOPT_NOSIGNAL, 1L);

    const CURLcode nCode = c->easy_perform(h);
    long nStatus = 0;
    c->easy_getinfo(h, CURLINFO_RESPONSE_CODE, &nStatus);
    c->easy_cleanup(h);
    c->slist_free_all(pHeaders);

    if (t.bStopped) { if (sError.empty()) sError = "Stopped"; return false; }
    if (nCode != CURLE_OK)
    {
        sError = "Could not reach GitHub";
        return false;
    }
    if (nStatus != 200)
    {
        if (nStatus == 403 || nStatus == 429) sError = "GitHub is limiting requests. Try again later.";
        else if (nStatus == 404) sError = "GitHub has no release to offer.";
        else sError = "GitHub answered " + std::to_string(nStatus);
        return false;
    }
    return true;
}

} // namespace

void SetHttpUserAgent(const std::string &sAgent)
{
    g_sAgent = sAgent;
}

bool HttpGet(const std::string &sUrl, const std::string &sAccept, std::string &sOut, size_t nMax,
             std::string &sError)
{
    sOut.clear();
    return Get(sUrl, sAccept, [&](const char *p, size_t n, uint64_t) {
        if (sOut.size() + n > nMax) { sError = "GitHub's answer was too long"; return false; }
        sOut.append(p, n);
        return true;
    }, sError);
}

bool HttpDownload(const std::string &sUrl, const std::string &sPath, const HttpProgress &progress,
                  std::string &sError)
{
    FILE *f = std::fopen(sPath.c_str(), "wb");
    if (!f) { sError = "Could not save the download in the launcher's folder"; return false; }
    uint64_t nDone = 0;
    bool bOk = Get(sUrl, "application/octet-stream", [&](const char *p, size_t n, uint64_t nTotal) {
        if (std::fwrite(p, 1, n, f) != n) { sError = "Could not save the download in the launcher's folder"; return false; }
        nDone += n;
        return !progress || progress(nDone, nTotal);
    }, sError);
    if (std::fclose(f) != 0 && bOk) { sError = "Could not save the download in the launcher's folder"; bOk = false; }
    return bOk;
}

bool OpenInBrowser(const std::string &sUrl)
{
    if (sUrl.compare(0, 19, "https://github.com/") != 0) return false;
    for (char c : sUrl)
        if ((unsigned char)c <= ' ' || c == '"') return false;
    // Forked twice, so the browser isn't left as this process's child to reap
    const pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0)
    {
        if (fork() == 0)
        {
            execlp("xdg-open", "xdg-open", sUrl.c_str(), (char *)nullptr);
            _exit(127);
        }
        _exit(0);
    }
    int nStatus = 0;
    waitpid(pid, &nStatus, 0);
    return true;
}

} // namespace ltlaunch
