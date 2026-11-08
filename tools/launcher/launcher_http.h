#ifndef LTLAUNCH_HTTP_H
#define LTLAUNCH_HTTP_H

#include <cstdint>
#include <functional>
#include <string>

namespace ltlaunch {

// Bytes received so far and the total, 0 when the server didn't say. Returning false stops the transfer
typedef std::function<bool(uint64_t, uint64_t)> HttpProgress;

// An HTTPS GET into memory, refused past nMax bytes. sAccept is the Accept header
bool HttpGet(const std::string &sUrl, const std::string &sAccept, std::string &sOut, size_t nMax,
             std::string &sError);

// An HTTPS GET into a file, following redirects
bool HttpDownload(const std::string &sUrl, const std::string &sPath, const HttpProgress &progress,
                  std::string &sError);

// A file's SHA-256 in lowercase hex
bool Sha256File(const std::string &sPath, std::string &sHex, std::string &sError);

// Opens a GitHub page in the default browser. Any other address is refused
bool OpenInBrowser(const std::string &sUrl);

// The User-Agent every request sends, which GitHub's API requires
void SetHttpUserAgent(const std::string &sAgent);

} // namespace ltlaunch

#endif
