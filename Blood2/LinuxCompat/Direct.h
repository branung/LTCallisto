// Linux stand-in for MSVC's <direct.h>: the directory calls Blood 2's config and save code makes
#ifndef BLOOD2_LINUXCOMPAT_DIRECT_H
#define BLOOD2_LINUXCOMPAT_DIRECT_H
#include <unistd.h>
#include <sys/stat.h>
inline int _mkdir(const char *p) { return mkdir(p, 0755); }
// MSVC's one-argument mkdir, which MenuCharacter.cpp calls to make "players"
inline int mkdir(const char *p) { return mkdir(p, 0755); }
inline char *_getcwd(char *b, int n) { return getcwd(b, (size_t)n); }
inline int _chdir(const char *p) { return chdir(p); }
#endif
