// Linux stand-in for MSVC's <mbstring.h>. Blood 2 ran in a single-byte code page, where each _mbs function
// is its str counterpart on unsigned char. sdk/inc/ltbasedefs.h already defines the others on Linux,
// so this has only the six it lacks and none of its names, and the two can be included in either order
#ifndef BLOOD2_LINUXCOMPAT_MBSTRING_H
#define BLOOD2_LINUXCOMPAT_MBSTRING_H
#include <string.h>
#include <ctype.h>
#include <stddef.h>

inline unsigned char *_mbscat(unsigned char *d, const unsigned char *s) { return (unsigned char *)strcat((char *)d, (const char *)s); }
inline unsigned char *_mbsncat(unsigned char *d, const unsigned char *s, size_t n) { return (unsigned char *)strncat((char *)d, (const char *)s, n); }
inline int _mbsncmp(const unsigned char *a, const unsigned char *b, size_t n) { return strncmp((const char *)a, (const char *)b, n); }
inline unsigned char *_mbsstr(const unsigned char *h, const unsigned char *n) { return (unsigned char *)strstr((const char *)h, (const char *)n); }
inline unsigned char *_mbschr(const unsigned char *s, unsigned int c) { return (unsigned char *)strchr((const char *)s, (int)c); }
inline unsigned char *_mbslwr(unsigned char *s) { for (unsigned char *p = s; *p; ++p) *p = (unsigned char)tolower(*p); return s; }
#endif
