#include "launcher_update.h"
#include "launcher_sign.h"

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

#if defined(_WIN64)
const char *kReleaseArch = "win64";
const char *kReleaseArchive = ".zip";
const char *kLauncherFile = "LTCallisto.exe";
#elif defined(_WIN32)
const char *kReleaseArch = "win32";
const char *kReleaseArchive = ".zip";
const char *kLauncherFile = "LTCallisto.exe";
#else
const char *kReleaseArch = "linux64";
const char *kReleaseArchive = ".tar.gz";
const char *kLauncherFile = "LTCallisto";
#endif

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

// SHA-256 (FIPS 180-4), so both platforms check downloads the same way
struct Sha256
{
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    unsigned char block[64];
    size_t nBlock = 0;
    uint64_t nBits = 0;

    static uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void Compress(const unsigned char *p)
    {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
        for (int i = 16; i < 64; ++i)
        {
            const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i)
        {
            const uint32_t t1 = hh + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void Update(const unsigned char *p, size_t n)
    {
        nBits += (uint64_t)n * 8;
        while (n)
        {
            const size_t nTake = (64 - nBlock) < n ? (64 - nBlock) : n;
            std::memcpy(block + nBlock, p, nTake);
            nBlock += nTake; p += nTake; n -= nTake;
            if (nBlock == 64) { Compress(block); nBlock = 0; }
        }
    }

    std::string Hex()
    {
        const uint64_t nLength = nBits;
        const unsigned char cPad = 0x80, cZero = 0;
        Update(&cPad, 1);
        while (nBlock != 56) Update(&cZero, 1);
        unsigned char len[8];
        for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(nLength >> (56 - i * 8));
        Update(len, 8);
        std::string s;
        char sByte[3];
        for (uint32_t v : h)
            for (int i = 3; i >= 0; --i)
            {
                std::snprintf(sByte, sizeof(sByte), "%02x", (unsigned)((v >> (i * 8)) & 0xff));
                s += sByte;
            }
        return s;
    }
};

// One tar entry's header, with any GNU long name or pax path that came before it applied
struct TarEntry
{
    std::string m_sName;
    char m_cType = 0;
    unsigned m_nMode = 0;
    uint64_t m_nSize = 0;
};

uint64_t TarOctal(const unsigned char *p, size_t n)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n && p[i]; ++i)
        if (p[i] >= '0' && p[i] <= '7') v = v * 8 + (p[i] - '0');
    return v;
}

bool GzRead(gzFile gz, void *p, size_t n)
{
    return n == 0 || gzread(gz, p, (unsigned)n) == (int)n;
}

bool GzSkip(gzFile gz, uint64_t n)
{
    return n == 0 || gzseek(gz, (z_off_t)n, SEEK_CUR) != -1;
}

uint64_t TarPadding(uint64_t n)
{
    return (512 - n % 512) % 512;
}

// Reads the next entry's header. 1 for an entry, 0 at the end, -1 when the archive is damaged
int NextTarEntry(gzFile gz, TarEntry &e)
{
    std::string sLongName;
    for (;;)
    {
        unsigned char h[512];
        if (!GzRead(gz, h, sizeof(h))) return -1;
        bool bZero = true;
        for (unsigned char c : h) if (c) { bZero = false; break; }
        if (bZero) return 0;

        // The checksum counts its own field as spaces
        unsigned nSum = 0;
        for (int i = 0; i < 512; ++i) nSum += (i >= 148 && i < 156) ? ' ' : h[i];
        if (nSum != TarOctal(h + 148, 8)) return -1;

        e = TarEntry();
        e.m_cType = (char)h[156];
        e.m_nMode = (unsigned)TarOctal(h + 100, 8);
        e.m_nSize = TarOctal(h + 124, 12);
        e.m_sName.assign((const char *)h, strnlen((const char *)h, 100));
        if (std::memcmp(h + 257, "ustar", 5) == 0 && h[345])
            e.m_sName = std::string((const char *)h + 345, strnlen((const char *)h + 345, 155)) + "/" + e.m_sName;

        // A GNU long name or a pax record holds the next entry's real name
        if (e.m_cType == 'L' || e.m_cType == 'x' || e.m_cType == 'g')
        {
            if (e.m_nSize > 1024 * 1024) return -1;
            std::string sData((size_t)e.m_nSize, '\0');
            if (!GzRead(gz, &sData[0], sData.size()) || !GzSkip(gz, TarPadding(e.m_nSize))) return -1;
            if (e.m_cType == 'L')
                sLongName = sData.c_str();
            else if (e.m_cType == 'x')
            {
                // Records of "<length> <key>=<value>\n"
                for (size_t i = 0; i < sData.size();)
                {
                    const size_t nLen = (size_t)std::strtoul(sData.c_str() + i, nullptr, 10);
                    if (nLen == 0 || i + nLen > sData.size()) break;
                    const std::string sRec = sData.substr(i, nLen);
                    const size_t nSpace = sRec.find(' '), nEq = sRec.find('=');
                    if (nSpace != std::string::npos && nEq != std::string::npos && sRec.compare(nSpace + 1, nEq - nSpace - 1, "path") == 0)
                        sLongName = sRec.substr(nEq + 1, sRec.size() - nEq - 2);
                    i += nLen;
                }
            }
            continue;
        }
        if (!sLongName.empty()) e.m_sName = sLongName;
        if (e.m_cType == '5' && !e.m_sName.empty() && e.m_sName.back() != '/') e.m_sName += '/';
        return 1;
    }
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

bool ParseRelease(const std::string &sJson, const std::string &sArch, const std::string &sArchive,
                  ReleaseInfo &out, std::string &sError)
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

    // Only the package named for this release's own stamp, which also keeps the name safe to save under
    const std::string sStamp = out.m_sTag.substr(out.m_sTag[0] == 'v' || out.m_sTag[0] == 'V' ? 1 : 0);
    const std::string sPackage = "ltcallisto-" + Lower(sArch) + "_v" + sStamp + Lower(sArchive);
    const auto assets = j.find("assets");
    if (assets != j.end() && assets->is_array())
    {
        for (const nlohmann::json &a : *assets)
        {
            if (!a.is_object()) continue;
            const std::string sName = JsonString(a, "name");
            if (Lower(sName) != sPackage) continue;

            out.m_sAssetName = sName;
            out.m_sAssetUrl = JsonString(a, "browser_download_url");
            const auto size = a.find("size");
            if (size != a.end() && size->is_number_unsigned()) out.m_nAssetSize = size->get<uint64_t>();
            const std::string sDigest = JsonString(a, "digest");
            if (sDigest.compare(0, 7, "sha256:") == 0) out.m_sSha256 = Lower(sDigest.substr(7));
            break;
        }
        for (const nlohmann::json &a : *assets)
        {
            if (!a.is_object() || Lower(JsonString(a, "name")) != sPackage + ".sig") continue;
            const std::string sUrl = JsonString(a, "browser_download_url");
            if (sUrl.compare(0, 8, "https://") == 0) out.m_sSigUrl = sUrl;
            break;
        }
    }

    if (out.m_sAssetUrl.compare(0, 8, "https://") != 0)
    {
        sError = "The latest release has no " + sArch + " package";
        return false;
    }
    return true;
}

namespace {

bool FromHex(const std::string &sHex, unsigned char *pOut, size_t nBytes)
{
    if (sHex.size() != nBytes * 2) return false;
    for (size_t i = 0; i < nBytes * 2; ++i)
    {
        const char c = (char)std::tolower((unsigned char)sHex[i]);
        const int n = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
        if (n < 0) return false;
        if (i % 2 == 0) pOut[i / 2] = (unsigned char)(n << 4);
        else pOut[i / 2] |= (unsigned char)n;
    }
    return true;
}

} // namespace

bool CheckReleaseSignature(const std::string &sSig, const std::string &sKeyHex, const std::string &sPackage,
                           std::string &sSha256, std::string &sError)
{
    std::string sFile, sHash, sSignature;
    std::istringstream in(sSig);
    std::string sLine;
    while (std::getline(in, sLine))
    {
        if (!sLine.empty() && sLine.back() == '\r') sLine.pop_back();
        if (sLine.compare(0, 5, "file=") == 0) sFile = sLine.substr(5);
        else if (sLine.compare(0, 7, "sha256=") == 0) sHash = Lower(sLine.substr(7));
        else if (sLine.compare(0, 8, "ed25519=") == 0) sSignature = sLine.substr(8);
    }

    unsigned char key[32], sig[64], hash[32];
    if (!FromHex(sKeyHex, key, sizeof(key)))
    {
        sError = "This launcher has no key to check updates with";
        return false;
    }
    if (sFile != sPackage || !FromHex(sHash, hash, sizeof(hash)) || !FromHex(sSignature, sig, sizeof(sig)))
    {
        sError = "The release's signature is not for this package";
        return false;
    }

    // sign_release.py signs the same text
    const std::string sSigned = "LTCallisto release signature 1\nfile=" + sFile + "\nsha256=" + sHash + "\n";
    if (!Ed25519Verify(sig, (const unsigned char *)sSigned.data(), sSigned.size(), key))
    {
        sError = "The release's signature doesn't match LTCallisto's key";
        return false;
    }
    sSha256 = sHash;
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

bool ExtractTarGz(const std::string &sTarGz, const std::string &sDestDir, std::vector<std::string> &files,
                  std::string &sError)
{
    files.clear();
    gzFile gz = gzopen(sTarGz.c_str(), "rb");
    if (!gz) { sError = "Could not open the downloaded update"; return false; }

    // The folder every entry sits in, which isn't unpacked
    std::string sTop;
    bool bFirst = true;
    TarEntry e;
    int nResult;
    while ((nResult = NextTarEntry(gz, e)) == 1)
    {
        const std::string sName = SafeName(e.m_sName);
        const size_t nSlash = sName.find('/');
        const std::string sFirst = nSlash == std::string::npos ? std::string() : sName.substr(0, nSlash + 1);
        if (bFirst) { sTop = sFirst; bFirst = false; }
        else if (sFirst != sTop) sTop.clear();
        if (!GzSkip(gz, e.m_nSize + TarPadding(e.m_nSize))) { nResult = -1; break; }
    }
    if (nResult < 0 || gzrewind(gz) != 0) { gzclose(gz); sError = "The update's archive is damaged"; return false; }

    std::error_code ec;
    const fs::path dest(sDestDir);
    fs::create_directories(dest, ec);
    std::vector<char> buf(64 * 1024);
    bool bOk = true;
    while (bOk && (nResult = NextTarEntry(gz, e)) == 1)
    {
        std::string sName = SafeName(e.m_sName);
        if (sName.empty()) { sError = "The update holds a file named " + e.m_sName; bOk = false; break; }
        sName.erase(0, sTop.size());

        if (e.m_cType == '5')
        {
            if (!sName.empty())
            {
                fs::create_directories(dest / fs::path(sName), ec);
                files.push_back(sName);
            }
            continue;
        }
        if (e.m_cType != '0' && e.m_cType != '\0')
        {
            sError = "The update holds " + sName + ", which isn't a plain file";
            bOk = false;
            break;
        }

        const fs::path out = dest / fs::path(sName);
        fs::create_directories(out.parent_path(), ec);
        std::ofstream f(out, std::ios::binary | std::ios::trunc);
        uint64_t nLeft = e.m_nSize;
        while (f && nLeft)
        {
            const size_t nChunk = nLeft < buf.size() ? (size_t)nLeft : buf.size();
            if (!GzRead(gz, &buf[0], nChunk)) { nLeft = 1; break; }
            f.write(&buf[0], (std::streamsize)nChunk);
            nLeft -= nChunk;
        }
        f.close();
        if (nLeft || !f || !GzSkip(gz, TarPadding(e.m_nSize)))
        {
            sError = "Could not unpack " + sName;
            bOk = false;
            break;
        }
#ifndef _WIN32
        // Programs stay executable, and only the owner can change them
        fs::permissions(out, fs::perms(e.m_nMode & 0755), ec);
#endif
        files.push_back(sName);
    }
    // zlib checks the gzip stream's CRC as it reaches the end
    if (bOk && nResult < 0) { sError = "The update's archive is damaged"; bOk = false; }
    gzclose(gz);
    return bOk;
}

bool ExtractArchive(const std::string &sArchive, const std::string &sDestDir, std::vector<std::string> &files,
                    std::string &sError)
{
    const std::string s = Lower(sArchive);
    if (EndsWith(s, ".tar.gz") || EndsWith(s, ".tgz")) return ExtractTarGz(sArchive, sDestDir, files, sError);
    return ExtractZip(sArchive, sDestDir, files, sError);
}

bool Sha256File(const std::string &sPath, std::string &sHex, std::string &sError)
{
    std::ifstream in(fs::path(sPath), std::ios::binary);
    if (!in) { sError = "Could not check the download"; return false; }
    Sha256 sha;
    std::vector<char> buf(64 * 1024);
    while (in)
    {
        in.read(&buf[0], (std::streamsize)buf.size());
        const std::streamsize n = in.gcount();
        if (n > 0) sha.Update((const unsigned char *)&buf[0], (size_t)n);
    }
    if (in.bad()) { sError = "Could not check the download"; return false; }
    sHex = sha.Hex();
    return true;
}

bool UpdateReplaces(const std::string &sRelPath)
{
    const std::string s = Lower(sRelPath);
    for (const char *pExt : { ".exe", ".dll", ".lto", ".fxd", ".ren", ".so" })
        if (EndsWith(s, pExt)) return true;
    // A versioned Linux library (libdxvk_d3d9.so.0), and a Linux program, which has no extension (Lithtech)
    if (s.find(".so.") != std::string::npos) return true;
    return fs::path(s).filename().string().find('.') == std::string::npos;
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

    // The list is read back from the Update folder, so its names are checked again
    for (const std::string &sRel : files)
        if (SafeName(sRel) != sRel)
        {
            sError = "The update holds a file named " + sRel;
            return false;
        }

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
