#ifndef LTLAUNCH_UPDATER_H
#define LTLAUNCH_UPDATER_H

#include "launcher_update.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ltlaunch {

// Checks GitHub for a newer LTCallisto, downloads and unpacks it, and installs it over this one.
// The network work runs on its own thread, which calls wake whenever the state changes
class Updater
{
public:
    enum class State { None, Available, Downloading, Ready, Failed };

    struct View
    {
        State m_eState = State::None;
        ReleaseInfo m_Release;
        uint64_t m_nDone = 0, m_nTotal = 0;
        std::string m_sError;
    };

    // sRepo is owner/name on GitHub. sCurrentBuild is this build's stamp
    Updater(const std::string &sRepo, const std::string &sInstallDir, const std::string &sCurrentBuild,
            const std::function<void()> &wake);
    ~Updater();

    void StartCheck();
    void StartDownload();

    // Moves the downloaded files into place. False with the reason in the view when nothing changed
    bool Install();

    View Get() const;

    // The releases page, for when there's no release to point at
    std::string ReleasesPage() const;

private:
    void Run(void (Updater::*pJob)());
    void Check();
    void Download();
    void Set(State eState, const std::string &sError = std::string());
    void CleanUpdateFolder();
    std::string UpdateDir() const;
    std::string StageDir(const ReleaseInfo &rel) const;
    std::string FilesList(const ReleaseInfo &rel) const;

    const std::string m_sRepo, m_sInstallDir, m_sCurrent;
    const std::function<void()> m_Wake;

    mutable std::mutex m_Lock;
    View m_View;
    std::vector<std::string> m_Files; // The unpacked files, once Ready

    std::thread m_Thread;
    std::atomic<bool> m_bCancel{ false };
};

} // namespace ltlaunch

#endif
