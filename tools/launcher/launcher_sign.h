#ifndef LTLAUNCH_SIGN_H
#define LTLAUNCH_SIGN_H

#include <cstddef>

namespace ltlaunch {

// SHA-512 (FIPS 180-4)
void Sha512(const unsigned char *pData, size_t nSize, unsigned char out[64]);

// Whether sig is key's Ed25519 signature (RFC 8032) of the message. Rejects the second encoding
// of a signature (S at or above the group order), so each message has one valid signature per key
bool Ed25519Verify(const unsigned char sig[64], const unsigned char *pMsg, size_t nMsg, const unsigned char key[32]);

} // namespace ltlaunch

#endif
