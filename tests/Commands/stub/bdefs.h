// What runtime/shared/src/conparse.cpp needs from the engine's bdefs.h, so the engine's own
// tokenizer compiles here without the engine
#ifndef __BDEFS_H__
#define __BDEFS_H__
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint32_t uint32;
typedef int32_t int32;
typedef unsigned int LTBOOL;
#define LTTRUE  1
#define LTFALSE 0
#define LTNULL  0
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif
#endif
