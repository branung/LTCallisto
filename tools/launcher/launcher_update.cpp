#include "launcher_update.h"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace ltlaunch {

const char *kUpdateFolder = "Update";

namespace {

// The suffix a replaced file is renamed to. Distinct, so a player's own .old files are never touched
const char *kOldSuffix = ".ltcupdate-old";

std::string Lower(std::string s)
{
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool EndsWith(const std::string &s, const std::string &sEnd)
{
    return s.size() >= sEnd.size() && s.compare(s.size() - sEnd.size(), sEnd.size(), sEnd) == 0;
}

std::string JsonString(const nlohmann::json &j, const char *pKey)
{
    const auto it = j.find(pKey);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

// Decodes one UTF-8 character at s[i], advancing i. A broken sequence reads as one byte
uint32_t NextCodePoint(const std::string &s, size_t &i)
{
    const unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int nMore = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
    uint32_t cp = c & (0x3F >> nMore);
    if (nMore == 0) return 0xFFFD;
    for (; nMore > 0; --nMore)
    {
        if (i >= s.size() || ((unsigned char)s[i] & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    }
    return cp;
}

} // namespace

std::string ToAscii(const std::string &s)
{
    std::string out;
    size_t i = 0;
    while (i < s.size())
    {
        const uint32_t cp = NextCodePoint(s, i);
        if (cp == '\t') out += "    ";
        else if (cp >= 32 && cp < 127) out += (char)cp;
        else if (cp == 0x2018 || cp == 0x2019 || cp == 0x201A || cp == 0x2032) out += '\'';
        else if (cp == 0x201C || cp == 0x201D || cp == 0x201E || cp == 0x2033) out += '"';
        else if (cp == 0x2010 || cp == 0x2011 || cp == 0x2012 || cp == 0x2013 || cp == 0x2212) out += '-';
        else if (cp == 0x2014 || cp == 0x2015) out += '-';
        else if (cp == 0x2026) out += "...";
        else if (cp == 0x2022 || cp == 0x00B7) out += '*';
        else if (cp == 0x00A0 || cp == 0x2009 || cp == 0x202F) out += ' ';
        else if (cp == 0x00D7) out += 'x';
        else if (cp == 0x2192) out += "->";
        else if (cp == 0x2190) out += "<-";
        else if (cp == 0x00A9) out += "(c)";
        else if (cp == 0x00AE) out += "(R)";
        else if (cp == 0x2122) out += "(TM)";
        else if (cp >= 32) out += '?';
    }
    return out;
}

namespace {

// Inline Markdown: images and links keep their text, and emphasis and code marks go
std::string StripInline(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size();)
    {
        // [text](url) and ![alt](url)
        const bool bImage = s[i] == '!' && i + 1 < s.size() && s[i + 1] == '[';
        if (s[i] == '[' || bImage)
        {
            const size_t nOpen = bImage ? i + 1 : i;
            const size_t nClose = s.find(']', nOpen);
            if (nClose != std::string::npos && nClose + 1 < s.size() && s[nClose + 1] == '(')
            {
                const size_t nEnd = s.find(')', nClose);
                if (nEnd != std::string::npos)
                {
                    out += s.substr(nOpen + 1, nClose - nOpen - 1);
                    i = nEnd + 1;
                    continue;
                }
            }
        }
        // <https://...>
        if (s[i] == '<' && s.compare(i + 1, 4, "http") == 0)
        {
            const size_t nEnd = s.find('>', i);
            if (nEnd != std::string::npos)
            {
                out += s.substr(i + 1, nEnd - i - 1);
                i = nEnd + 1;
                continue;
            }
        }
        if (s.compare(i, 2, "**") == 0 || s.compare(i, 2, "__") == 0 || s.compare(i, 2, "~~") == 0)
        {
            i += 2;
            continue;
        }
        if (s[i] == '`')
        {
            ++i;
            continue;
        }
        out += s[i++];
    }
    return out;
}

std::string TrimLeft(const std::string &s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

uint16_t Get16(const std::vector<unsigned char> &d, size_t i)
{
    return (uint16_t)(d[i] | (d[i + 1] << 8));
}

uint32_t Get32(const std::vector<unsigned char> &d, size_t i)
{
    return (uint32_t)d[i] | ((uint32_t)d[i + 1] << 8) | ((uint32_t)d[i + 2] << 16) | ((uint32_t)d[i + 3] << 24);
}

uint64_t Get64(const std::vector<unsigned char> &d, size_t i)
{
    return (uint64_t)Get32(d, i) | ((uint64_t)Get32(d, i + 4) << 32);
}

struct ZipEntry
{
    std::string m_sName;
    uint16_t m_nMethod = 0, m_nFlags = 0;
    uint32_t m_nCrc = 0;
    uint64_t m_nPacked = 0, m_nSize = 0, m_nLocal = 0;
};

bool ReadZipDirectory(const std::vector<unsigned char> &d, std::vector<ZipEntry> &entries, std::string &sError)
{
    sError = "The update's zip is damaged";
    if (d.size() < 22) return false;

    // The end of central directory record, searched for back from the end past any comment
    size_t nEnd = std::string::npos;
    const size_t nStop = d.size() > 22 + 65535 ? d.size() - 22 - 65535 : 0;
    for (size_t i = d.size() - 22 + 1; i-- > nStop;)
        if (Get32(d, i) == 0x06054b50) { nEnd = i; break; }
    if (nEnd == std::string::npos) return false;

    const uint16_t nCount = Get16(d, nEnd + 10);
    size_t nPos = Get32(d, nEnd + 16);
    for (uint16_t n = 0; n < nCount; ++n)
    {
        if (nPos + 46 > d.size() || Get32(d, nPos) != 0x02014b50) return false;
        ZipEntry e;
        e.m_nFlags = Get16(d, nPos + 8);
        e.m_nMethod = Get16(d, nPos + 10);
        e.m_nCrc = Get32(d, nPos + 16);
        e.m_nPacked = Get32(d, nPos + 20);
        e.m_nSize = Get32(d, nPos + 24);
        const uint16_t nName = Get16(d, nPos + 28), nExtra = Get16(d, nPos + 30), nComment = Get16(d, nPos + 32);
        e.m_nLocal = Get32(d, nPos + 42);
        if (nPos + 46 + nName + nExtra > d.size()) return false;
        e.m_sName.assign((const char *)&d[nPos + 46], nName);

        // Zip64 sizes and offset, in that order, for whichever fields are full
        for (size_t x = nPos + 46 + nName; x + 4 <= nPos + 46 + nName + nExtra;)
        {
            const uint16_t nId = Get16(d, x), nLen = Get16(d, x + 2);
            if (nId == 0x0001)
            {
                size_t f = x + 4;
                if (e.m_nSize == 0xFFFFFFFF && f + 8 <= x + 4 + nLen) { e.m_nSize = Get64(d, f); f += 8; }
                if (e.m_nPacked == 0xFFFFFFFF && f + 8 <= x + 4 + nLen) { e.m_nPacked = Get64(d, f); f += 8; }
                if (e.m_nLocal == 0xFFFFFFFF && f + 8 <= x + 4 + nLen) { e.m_nLocal = Get64(d, f); }
            }
            x += 4 + nLen;
        }

        entries.push_back(e);
        nPos += 46 + nName + nExtra + nComment;
    }
    sError.clear();
    return true;
}

bool Inflate(const unsigned char *pIn, uint64_t nIn, std::vector<unsigned char> &out)
{
    z_stream z;
    std::memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK) return false;
    z.next_in = (Bytef *)pIn;
    z.avail_in = (uInt)nIn;
    z.next_out = out.empty() ? 0 : &out[0];
    z.avail_out = (uInt)out.size();
    const int nResult = inflate(&z, Z_FINISH);
    const bool bOk = nResult == Z_STREAM_END && z.total_out == out.size();
    inflateEnd(&z);
    return bOk;
}

// A zip entry's name as a path inside the destination, or empty when it would leave it
std::string SafeName(std::string sName)
{
    for (char &c : sName)
        if (c == '\\') c = '/';
    if (sName.empty() || sName[0] == '/' || sName.find(':') != std::string::npos) return std::string();
    std::stringstream ss(sName);
    std::string sPart;
    while (std::getline(ss, sPart, '/'))
        if (sPart == "..") return std::string();
    return sName;
}

bool WriteFile(const fs::path &path, const unsigned char *p, size_t n)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (n) out.write((const char *)p, (std::streamsize)n);
    return (bool)out;
}

} // namespace

bool ParseBuildStamp(const std::string &sStamp, int nOut[3])
{
    std::string s = sStamp;
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    int a = 0, b = 0, c = 0;
    char cExtra = 0;
    if (std::sscanf(s.c_str(), "%d.%d.%d%c", &a, &b, &c, &cExtra) != 3) return false;
    if (a < 2000 || b < 101 || b > 1231 || c < 0 || c > 235959) return false;
    nOut[0] = a; nOut[1] = b; nOut[2] = c;
    return true;
}

bool IsNewerBuild(const std::string &sRelease, const std::string &sCurrent)
{
    int r[3], c[3];
    if (!ParseBuildStamp(sRelease, r) || !ParseBuildStamp(sCurrent, c)) return false;
    for (int i = 0; i < 3; ++i)
        if (r[i] != c[i]) return r[i] > c[i];
    return false;
}

bool ParseRelease(const std::string &sJson, const std::string &sArch, ReleaseInfo &out, std::string &sError)
{
    const nlohmann::json j = nlohmann::json::parse(sJson, nullptr, false);
    if (j.is_discarded() || !j.is_object())
    {
        sError = "GitHub's answer could not be read";
        return false;
    }

    out = ReleaseInfo();
    out.m_sTag = JsonString(j, "tag_name");
    out.m_sName = JsonString(j, "name");
    out.m_sBody = JsonString(j, "body");
    out.m_sPageUrl = JsonString(j, "html_url");
    int nStamp[3];
    if (!ParseBuildStamp(out.m_sTag, nStamp))
    {
        sError = "The latest release's tag is not a build stamp";
        return false;
    }

    const std::string sPrefix = "ltcallisto-" + Lower(sArch) + "_";
    const auto assets = j.find("assets");
    if (assets != j.end() && assets->is_array())
        for (const nlohmann::json &a : *assets)
        {
            if (!a.is_object()) continue;
            const std::string sName = JsonString(a, "name");
            const std::string sLower = Lower(sName);
            if (sLower.compare(0, sPrefix.size(), sPrefix) != 0 || !EndsWith(sLower, ".zip")) continue;

            out.m_sAssetName = sName;
            out.m_sAssetUrl = JsonString(a, "browser_download_url");
            const auto size = a.find("size");
            if (size != a.end() && size->is_number_unsigned()) out.m_nAssetSize = size->get<uint64_t>();
            const std::string sDigest = JsonString(a, "digest");
            if (sDigest.compare(0, 7, "sha256:") == 0) out.m_sSha256 = Lower(sDigest.substr(7));
            break;
        }

    if (out.m_sAssetUrl.compare(0, 8, "https://") != 0)
    {
        sError = "The latest release has no " + sArch + " zip";
        return false;
    }
    return true;
}

std::vector<NoteLine> ReleaseNotes(const std::string &sBody)
{
    std::vector<NoteLine> lines;
    std::stringstream ss(sBody.substr(0, 20000));
    std::string sLine;
    bool bFence = false;
    while (std::getline(ss, sLine) && lines.size() < 500)
    {
        if (!sLine.empty() && sLine.back() == '\r') sLine.pop_back();
        std::string s = TrimLeft(ToAscii(sLine));

        if (s.compare(0, 3, "```") == 0) { bFence = !bFence; continue; }

        NoteLine line;
        if (!bFence)
        {
            // Rules (---, ***, ___) become a gap
            if (s.size() >= 3 && s.find_first_not_of(s[0]) == std::string::npos &&
                (s[0] == '-' || s[0] == '*' || s[0] == '_'))
                s.clear();

            if (!s.empty() && s[0] == '#')
            {
                const size_t n = s.find_first_not_of('#');
                if (n != std::string::npos && n <= 6 && s[n] == ' ')
                {
                    s = TrimLeft(s.substr(n));
                    line.m_bHeading = true;
                }
            }
            while (!s.empty() && s[0] == '>') s = TrimLeft(s.substr(1));
            if (s.size() >= 2 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && s[1] == ' ')
            {
                s = TrimLeft(s.substr(2));
                line.m_bBullet = true;
            }
            s = StripInline(s);
        }

        while (!s.empty() && s.back() == ' ') s.pop_back();
        line.m_sText = s;

        // One blank line between paragraphs, and none at the start
        if (s.empty() && (lines.empty() || lines.back().m_sText.empty())) continue;
        lines.push_back(line);
    }
    while (!lines.empty() && lines.back().m_sText.empty()) lines.pop_back();
    return lines;
}

bool ExtractZip(const std::string &sZip, const std::string &sDestDir, std::vector<std::string> &files,
                std::string &sError)
{
    files.clear();
    std::vector<unsigned char> d;
    {
        std::ifstream in(fs::path(sZip), std::ios::binary | std::ios::ate);
        if (!in) { sError = "Could not open the downloaded update"; return false; }
        const std::streamsize n = in.tellg();
        if (n <= 0) { sError = "The downloaded update is empty"; return false; }
        d.resize((size_t)n);
        in.seekg(0);
        if (!in.read((char *)&d[0], n)) { sError = "Could not read the downloaded update"; return false; }
    }

    std::vector<ZipEntry> entries;
    if (!ReadZipDirectory(d, entries, sError)) return false;

    // The folder every entry sits in, which isn't unpacked
    std::string sTop;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const std::string sName = SafeName(entries[i].m_sName);
        const size_t nSlash = sName.find('/');
        const std::string sFirst = nSlash == std::string::npos ? std::string() : sName.substr(0, nSlash + 1);
        if (i == 0) sTop = sFirst;
        else if (sFirst != sTop) { sTop.clear(); break; }
    }

    std::error_code ec;
    const fs::path dest(sDestDir);
    fs::create_directories(dest, ec);

    for (const ZipEntry &e : entries)
    {
        std::string sName = SafeName(e.m_sName);
        if (sName.empty()) { sError = "The update holds a file named " + e.m_sName; return false; }
        sName.erase(0, sTop.size());
        if (sName.empty()) continue;

        if (sName.back() == '/')
        {
            fs::create_directories(dest / fs::path(sName), ec);
            files.push_back(sName);
            continue;
        }

        if (e.m_nFlags & 1) { sError = "The update's zip is encrypted"; return false; }
        if (e.m_nLocal + 30 > d.size() || Get32(d, (size_t)e.m_nLocal) != 0x04034b50)
        { sError = "The update's zip is damaged"; return false; }
        const uint64_t nData = e.m_nLocal + 30 + Get16(d, (size_t)e.m_nLocal + 26) + Get16(d, (size_t)e.m_nLocal + 28);
        if (nData + e.m_nPacked > d.size() || e.m_nSize > 512u * 1024u * 1024u)
        { sError = "The update's zip is damaged"; return false; }

        std::vector<unsigned char> data((size_t)e.m_nSize);
        if (e.m_nMethod == 0)
        {
            if (e.m_nPacked != e.m_nSize) { sError = "The update's zip is damaged"; return false; }
            if (e.m_nSize) std::memcpy(&data[0], &d[(size_t)nData], (size_t)e.m_nSize);
        }
        else if (e.m_nMethod == 8)
        {
            if (!Inflate(&d[(size_t)nData], e.m_nPacked, data))
            { sError = "Could not unpack " + sName; return false; }
        }
        else { sError = "The update's zip uses an unknown compression"; return false; }

        const uLong nCrc = crc32(crc32(0L, Z_NULL, 0), data.empty() ? Z_NULL : &data[0], (uInt)data.size());
        if (nCrc != e.m_nCrc) { sError = sName + " failed its check"; return false; }

        const fs::path out = dest / fs::path(sName);
        fs::create_directories(out.parent_path(), ec);
        if (!WriteFile(out, data.empty() ? 0 : &data[0], data.size()))
        { sError = "Could not unpack " + sName; return false; }
        files.push_back(sName);
    }
    return true;
}

bool UpdateReplaces(const std::string &sRelPath)
{
    const std::string s = Lower(sRelPath);
    for (const char *pExt : { ".exe", ".dll", ".lto", ".fxd", ".ren" })
        if (EndsWith(s, pExt)) return true;
    return false;
}

bool ApplyUpdate(const std::string &sStageDir, const std::vector<std::string> &files,
                 const std::string &sInstallDir, std::string &sError)
{
    struct Step
    {
        fs::path src, dst, old; // old is empty when dst was added
        bool bMoved = false;
    };
    std::vector<Step> steps;
    std::error_code ec;

    auto Undo = [&steps]()
    {
        std::error_code e;
        for (size_t i = steps.size(); i-- > 0;)
        {
            const Step &s = steps[i];
            if (s.bMoved) fs::rename(s.dst, s.src, e);
            if (!s.old.empty()) fs::rename(s.old, s.dst, e);
        }
    };

    for (const std::string &sRel : files)
    {
        const fs::path dst = fs::path(sInstallDir) / fs::path(sRel);
        if (!sRel.empty() && sRel.back() == '/')
        {
            fs::create_directories(dst, ec);
            continue;
        }

        Step step;
        step.src = fs::path(sStageDir) / fs::path(sRel);
        step.dst = dst;
        if (fs::exists(dst, ec))
        {
            if (!UpdateReplaces(sRel)) continue; // The player's copy stays

            // A leftover from an earlier update that's still in use gets the next free name
            for (int n = 0; n < 100; ++n)
            {
                fs::path old = dst;
                old += kOldSuffix;
                if (n) old += std::to_string(n);
                fs::remove(old, ec);
                if (!fs::exists(old, ec)) { step.old = old; break; }
            }
            if (step.old.empty() || (fs::rename(dst, step.old, ec), ec))
            {
                Undo();
                sError = "Could not replace " + sRel + ". Close any game that's running and try again";
                return false;
            }
        }
        else
            fs::create_directories(dst.parent_path(), ec);

        steps.push_back(step);
        fs::rename(step.src, step.dst, ec);
        if (ec)
        {
            Undo();
            sError = "Could not install " + sRel;
            return false;
        }
        steps.back().bMoved = true;
    }
    return true;
}

int RemoveOldFiles(const std::string &sInstallDir)
{
    int nLeft = 0;
    std::error_code ec;
    const fs::path root(sInstallDir);
    // The launcher's folder, Games, each game's folder and the folders inside those
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec))
    {
        if (it.depth() >= 3) it.disable_recursion_pending();
        if (!it->is_regular_file(ec)) continue;
        const std::string sName = it->path().filename().string();
        const size_t n = sName.find(kOldSuffix);
        if (n == std::string::npos || sName.find_first_not_of("0123456789", n + std::strlen(kOldSuffix)) != std::string::npos)
            continue;
        std::error_code e;
        if (!fs::remove(it->path(), e) || e) ++nLeft;
    }
    return nLeft;
}

bool InBuildTree(const std::string &sDir)
{
    std::error_code ec;
    fs::path p = fs::absolute(fs::path(sDir), ec);
    for (int i = 0; i < 5 && !p.empty(); ++i)
    {
        if (fs::exists(p / "CMakeCache.txt", ec)) return true;
        const fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return false;
}

} // namespace ltlaunch
