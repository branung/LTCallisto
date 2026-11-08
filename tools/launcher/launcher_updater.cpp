#include "launcher_updater.h"
#include "launcher_core.h"
#include "launcher_http.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace ltlaunch {

namespace {

// GitHub allows 60 unauthenticated API calls an hour, so an answer is reused for this long
const long long kCheckInterval = 4 * 60 * 60;

const char *kCacheFile = "latest.txt";

// The v<stamp> in a name like v2026.1106.120000.files or LTCallisto-win64_v2026.1106.120000.zip
std::string TagIn(const std::string &sName)
{
    for (size_t i = sName.find_first_of("vV"); i != std::string::npos; i = sName.find_first_of("vV", i + 1))
    {
        const size_t nEnd = sName.find_first_not_of("0123456789.", i + 1);
        std::string sTag = sName.substr(i, nEnd == std::string::npos ? std::string::npos : nEnd - i);
        while (!sTag.empty() && sTag.back() == '.') sTag.pop_back();
        int nStamp[3];
        if (ParseBuildStamp(sTag, nStamp)) return sTag;
    }
    return std::string();
}

} // namespace

Updater::Updater(const std::string &sRepo, const std::string &sInstallDir, const std::string &sCurrentBuild,
                 const std::function<void()> &wake)
    : m_sRepo(sRepo), m_sInstallDir(sInstallDir), m_sCurrent(sCurrentBuild), m_Wake(wake)
{
    SetHttpUserAgent(std::string("LTCallisto-Launcher/") + sCurrentBuild);
}

Updater::~Updater()
{
    m_bCancel = true;
    if (m_Thread.joinable()) m_Thread.join();
}

void Updater::Run(void (Updater::*pJob)())
{
    if (m_Thread.joinable()) m_Thread.join();
    m_Thread = std::thread(pJob, this);
}

void Updater::StartCheck()
{
    Run(&Updater::Check);
}

void Updater::StartDownload()
{
    {
        std::lock_guard<std::mutex> lock(m_Lock);
        if (m_View.m_eState != State::Available && m_View.m_eState != State::Failed) return;
        if (m_View.m_Release.m_sAssetUrl.empty()) return;
        m_View.m_eState = State::Downloading;
        m_View.m_nDone = m_View.m_nTotal = 0;
        m_View.m_sError.clear();
    }
    Run(&Updater::Download);
}

Updater::View Updater::Get() const
{
    std::lock_guard<std::mutex> lock(m_Lock);
    return m_View;
}

std::string Updater::ReleasesPage() const
{
    return "https://github.com/" + m_sRepo + "/releases";
}

void Updater::Set(State eState, const std::string &sError)
{
    {
        std::lock_guard<std::mutex> lock(m_Lock);
        m_View.m_eState = eState;
        m_View.m_sError = sError;
    }
    if (m_Wake) m_Wake();
}

std::string Updater::UpdateDir() const
{
    return JoinPath(m_sInstallDir, kUpdateFolder);
}

std::string Updater::StageDir(const ReleaseInfo &rel) const
{
    return JoinPath(UpdateDir(), rel.m_sTag);
}

std::string Updater::FilesList(const ReleaseInfo &rel) const
{
    return JoinPath(UpdateDir(), rel.m_sTag + ".files");
}

// Everything in the Update folder except the cached answer and what belongs to a release newer than this build
void Updater::CleanUpdateFolder()
{
    std::error_code ec;
    for (const fs::directory_entry &e : fs::directory_iterator(fs::path(UpdateDir()), ec))
    {
        const std::string sName = e.path().filename().string();
        if (sName == kCacheFile) continue;
        if (IsNewerBuild(TagIn(sName), m_sCurrent)) continue;
        std::error_code e2;
        fs::remove_all(e.path(), e2);
    }
}

void Updater::Check()
{
    // What a finished update left. The launcher that installed it may still be closing
    for (int i = 0; i < 10 && !m_bCancel && RemoveOldFiles(m_sInstallDir) > 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CleanUpdateFolder();

    // A recent answer is reused, otherwise GitHub is asked again
    const std::string sCache = JoinPath(UpdateDir(), kCacheFile);
    std::string sJson;
    {
        const std::string sCached = ReadTextFile(sCache);
        const size_t nLine = sCached.find('\n');
        if (sCached.compare(0, 8, "checked=") == 0 && nLine != std::string::npos)
        {
            const long long nThen = std::atoll(sCached.substr(8, nLine - 8).c_str());
            const long long nNow = (long long)std::time(nullptr);
            if (nNow >= nThen && nNow - nThen < kCheckInterval) sJson = sCached.substr(nLine + 1);
        }
    }
    if (sJson.empty())
    {
        std::string sError;
        if (!HttpGet("https://api.github.com/repos/" + m_sRepo + "/releases/latest", "application/vnd.github+json",
                     sJson, 1024 * 1024, sError))
            return; // Offline, or GitHub said no. Nothing is shown
        std::error_code ec;
        fs::create_directories(fs::path(UpdateDir()), ec);
        WriteTextFile(sCache, "checked=" + std::to_string((long long)std::time(nullptr)) + "\n" + sJson);
    }

    ReleaseInfo rel;
    std::string sError;
    if (!ParseRelease(sJson, kReleaseArch, kReleaseArchive, rel, sError) || !IsNewerBuild(rel.m_sTag, m_sCurrent))
        return;

    // Downloaded and unpacked by an earlier run
    std::vector<std::string> files;
    std::istringstream in(ReadTextFile(FilesList(rel)));
    std::string sLine;
    while (std::getline(in, sLine))
    {
        if (!sLine.empty() && sLine.back() == '\r') sLine.pop_back();
        if (!sLine.empty()) files.push_back(sLine);
    }
    const bool bReady = !files.empty() && DirExists(StageDir(rel));

    {
        std::lock_guard<std::mutex> lock(m_Lock);
        m_View.m_Release = rel;
        if (bReady) m_Files = files;
    }
    Set(bReady ? State::Ready : State::Available);
}

void Updater::Download()
{
    ReleaseInfo rel;
    {
        std::lock_guard<std::mutex> lock(m_Lock);
        rel = m_View.m_Release;
    }

    std::error_code ec;
    fs::create_directories(fs::path(UpdateDir()), ec);
    const std::string sZip = JoinPath(UpdateDir(), rel.m_sAssetName);
    const std::string sPart = sZip + ".part";

    int nLastPercent = -1;
    std::string sError;
    const bool bOk = HttpDownload(rel.m_sAssetUrl, sPart, [&](uint64_t nDone, uint64_t nTotal) {
        if (m_bCancel) return false;
        if (!nTotal) nTotal = rel.m_nAssetSize;
        {
            std::lock_guard<std::mutex> lock(m_Lock);
            m_View.m_nDone = nDone;
            m_View.m_nTotal = nTotal;
        }
        const int nPercent = nTotal ? (int)(nDone * 100 / nTotal) : 0;
        if (nPercent != nLastPercent && m_Wake) m_Wake();
        nLastPercent = nPercent;
        return true;
    }, sError);
    if (m_bCancel) { fs::remove(fs::path(sPart), ec); return; }
    if (!bOk) { fs::remove(fs::path(sPart), ec); Set(State::Failed, sError); return; }

    // Checked against what GitHub says it published
    if (rel.m_nAssetSize && fs::file_size(fs::path(sPart), ec) != rel.m_nAssetSize)
    {
        fs::remove(fs::path(sPart), ec);
        Set(State::Failed, "The download is incomplete");
        return;
    }
    if (!rel.m_sSha256.empty())
    {
        std::string sHash;
        if (!Sha256File(sPart, sHash, sError) || sHash != rel.m_sSha256)
        {
            fs::remove(fs::path(sPart), ec);
            Set(State::Failed, sError.empty() ? "The download is damaged" : sError);
            return;
        }
    }
    fs::remove(fs::path(sZip), ec);
    fs::rename(fs::path(sPart), fs::path(sZip), ec);
    if (ec) { Set(State::Failed, "Could not save the download in the launcher's folder"); return; }

    const std::string sStage = StageDir(rel);
    fs::remove_all(fs::path(sStage), ec);
    std::vector<std::string> files;
    if (!ExtractArchive(sZip, sStage, files, sError))
    {
        fs::remove_all(fs::path(sStage), ec);
        Set(State::Failed, sError);
        return;
    }
    fs::remove(fs::path(sZip), ec);

    // A package without the launcher at its top isn't an LTCallisto build
    if (!FileExists(JoinPath(sStage, kLauncherFile)))
    {
        fs::remove_all(fs::path(sStage), ec);
        Set(State::Failed, "The download is not an LTCallisto build");
        return;
    }

    std::string sList;
    for (const std::string &sFile : files) sList += sFile + "\n";
    if (!WriteTextFile(FilesList(rel), sList)) { Set(State::Failed, "Could not save the update in the launcher's folder"); return; }

    {
        std::lock_guard<std::mutex> lock(m_Lock);
        m_Files = files;
    }
    Set(State::Ready);
}

bool Updater::Install()
{
    ReleaseInfo rel;
    std::vector<std::string> files;
    {
        std::lock_guard<std::mutex> lock(m_Lock);
        if (m_View.m_eState != State::Ready) return false;
        rel = m_View.m_Release;
        files = m_Files;
    }

    std::string sError;
    if (!ApplyUpdate(StageDir(rel), files, m_sInstallDir, sError))
    {
        Set(State::Failed, sError);
        return false;
    }

    std::error_code ec;
    fs::remove(fs::path(FilesList(rel)), ec);
    fs::remove_all(fs::path(StageDir(rel)), ec);
    return true;
}

} // namespace ltlaunch
