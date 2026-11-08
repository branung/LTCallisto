// Linux stand-in for MSVC's <crtdbg.h>. Blood 2 uses only its assert macros, which become the C library's
#ifndef BLOOD2_LINUXCOMPAT_CRTDBG_H
#define BLOOD2_LINUXCOMPAT_CRTDBG_H
#include <assert.h>
#ifndef _ASSERT
#define _ASSERT(e) assert(e)
#endif
#ifndef _ASSERTE
#define _ASSERTE(e) assert(e)
#endif
#endif
