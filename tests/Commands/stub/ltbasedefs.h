// The SDK's ConParse and parse limits as sdk/inc/ltbasedefs.h declares them, for the engine's
// conparse.cpp and NOLF1's CommandMgr.cpp alike
#ifndef __LTBASEDEFS_H__
#define __LTBASEDEFS_H__
#include "bdefs.h"

#define PARSE_MAXTOKENS     64
#define PARSE_MAXTOKENSIZE  80
#define PARSE_MAXARGS       30
#define PARSE_MAXARGLEN     256

class ConParse {
public:
    ConParse() {m_pCommandPos = LTNULL;}
    ConParse(const char *pBuffer) {m_pCommandPos = pBuffer;}
    void Init(const char *pBuffer) {m_pCommandPos = pBuffer;}
    const char *m_Args[PARSE_MAXTOKENS];
    int m_nArgs;
public:
    bool Parse();
    bool ParseFind(const char *pLookFor, bool bCaseSensitive, uint32 minTokens=1);
private:
    const char *m_pCommandPos;
    char m_ArgBuffer[PARSE_MAXTOKENS*PARSE_MAXTOKENSIZE];
};
#endif
