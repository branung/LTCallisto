#ifndef LTLAUNCH_UPDATE_H
#define LTLAUNCH_UPDATE_H

#include <cstdint>
#include <string>
#include <vector>

namespace ltlaunch {

// This build's release package: the architecture in its name, the archive it comes in,
// and the launcher's file name inside it
extern const char *kReleaseArch;     // win64, win32 or linux64
extern const char *kReleaseArchive;  // .zip on Windows, .tar.gz on Linux, which keeps the programs executable
extern const char *kLauncherFile;    // LTCallisto.exe or LTCallisto

// The newest release on GitHub, and the package in it for this build's architecture
struct ReleaseInfo
{
    std::string m_sTag;       // v<year>.<month><day>.<time>, the build stamp of the package
    std::string m_sName;      // The release's title
    std::string m_sBody;      // Its notes, in Markdown
    std::string m_sPageUrl;   // The release's page
    std::string m_sAssetName; // LTCallisto-<arch>_v<stamp><archive>
    std::string m_sAssetUrl;
    std::string m_sSha256;    // Lowercase hex, empty when GitHub gave none
    uint64_t m_nAssetSize = 0;
};

// The three numbers of a build stamp, with or without its leading v. False for "unstamped" and the like
bool ParseBuildStamp(const std::string &sStamp, int nOut[3]);

// Whether sRelease is a later build than sCurrent. False when either isn't a stamp
bool IsNewerBuild(const std::string &sRelease, const std::string &sCurrent);

// Reads GitHub's releases/latest answer. False with sError set when it isn't usable,
// including when it has no package for sArch ending in sArchive
bool ParseRelease(const std::string &sJson, const std::string &sArch, const std::string &sArchive,
                  ReleaseInfo &out, std::string &sError);

// A file's SHA-256 in lowercase hex
bool Sha256File(const std::string &sPath, std::string &sHex, std::string &sError);

// One paragraph of the notes, with the Markdown taken out and only ASCII left, which is all the launcher's font draws
struct NoteLine
{
    std::string m_sText;
    bool m_bHeading = false;
    bool m_bBullet = false; // Drawn with a hanging indent
};

std::vector<NoteLine> ReleaseNotes(const std::string &sBody);

// UTF-8 as printable ASCII, with common punctuation given ASCII stand-ins
std::string ToAscii(const std::string &s);

// Unpacks a zip into sDestDir, dropping the folder every entry sits in when there is one.
// Checks each file's CRC. Refuses names that would land outside sDestDir.
// The files written are listed in files, relative to sDestDir with forward slashes
bool ExtractZip(const std::string &sZip, const std::string &sDestDir, std::vector<std::string> &files,
                std::string &sError);

// The same for a .tar.gz. On Linux each file keeps the permissions the archive gives it
bool ExtractTarGz(const std::string &sTarGz, const std::string &sDestDir, std::vector<std::string> &files,
                  std::string &sError);

// ExtractZip or ExtractTarGz, by the archive's name
bool ExtractArchive(const std::string &sArchive, const std::string &sDestDir, std::vector<std::string> &files,
                    std::string &sError);

// Whether an update replaces this file when it's already there: programs and modules.
// Configs and anything else the player may have changed are only added when missing
bool UpdateReplaces(const std::string &sRelPath);

// Moves the unpacked files into sInstallDir. A file being replaced is renamed to <name>.old first,
// which Windows allows for a running program. Puts everything back when a step fails
bool ApplyUpdate(const std::string &sStageDir, const std::vector<std::string> &files,
                 const std::string &sInstallDir, std::string &sError);

// Deletes the .old files a finished update left, skipping any still in use. Returns how many remain
int RemoveOldFiles(const std::string &sInstallDir);

// Whether a folder or one of its parents holds CMakeCache.txt, as a build's output folder does
bool InBuildTree(const std::string &sDir);

// The working folder updates download into, beside the launcher
extern const char *kUpdateFolder;

} // namespace ltlaunch

#endif
