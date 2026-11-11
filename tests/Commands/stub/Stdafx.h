// The server NOLF1's CommandMgr.cpp is compiled against. Every engine call it makes lands here,
// so the test can replay a script through it and record what it did.
#ifndef __STUB_STDAFX_H__
#define __STUB_STDAFX_H__
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "ltbasedefs.h"

typedef float LTFLOAT;
typedef int   LTRESULT;
#define LT_OK       0
#define LT_FINISHED 31
#ifndef _WIN32
#define _stricmp strcasecmp
#endif

typedef struct StubMessage* HMESSAGEREAD;
typedef struct StubMessage* HMESSAGEWRITE;
typedef struct StubString*  HSTRING;
typedef void* LPBASECLASS;

struct StubCommon
{
    LTRESULT Parse(ConParse *pParse) { return pParse->Parse() ? LT_OK : LT_FINISHED; }
};

struct StubServer
{
    float       fFrameTime;
    StubCommon  common;
    LTFLOAT     GetFrameTime() { return fFrameTime; }
    StubCommon* Common() { return &common; }
    void        CPrint(const char *pMsg, ...);
    // Save and load, unused by the replay
    LTFLOAT     ReadFromMessageFloat(HMESSAGEREAD) { return 0.0f; }
    HSTRING     ReadFromMessageHString(HMESSAGEREAD) { return LTNULL; }
    const char* GetStringData(HSTRING) { return ""; }
    void        FreeString(HSTRING) {}
    void        WriteToMessageFloat(HMESSAGEWRITE, LTFLOAT) {}
    HSTRING     CreateString(const char*) { return LTNULL; }
    void        WriteToMessageHString(HMESSAGEWRITE, HSTRING) {}
};
extern StubServer* g_pLTServer;

int   GetRandom(int lo, int hi);
float GetRandom(float lo, float hi);
#endif
