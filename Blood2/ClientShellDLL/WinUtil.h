#ifndef __WINUTIL_H
#define __WINUTIL_H

#ifndef _WIN32
// Linux: the stand-in's 32-bit DWORD comes before the typedef below,
// so every file declares CWinUtil with the type WinUtil.cpp defines it with
#include <windows.h>
#endif

#ifndef BOOL
	typedef int		    BOOL;
	#define FALSE		    0
	#define TRUE		    1
#endif

#ifndef DWORD
	typedef unsigned long	DWORD;
#endif

class CWinUtil
{
public:

	static BOOL GetMoviesPath (char* strPath);
	static BOOL DirExist (char* strPath);
	static BOOL CreateDir (char* strPath);
	static BOOL FileExist (char* strPath);

	static DWORD WinGetPrivateProfileString (char* lpAppName, char* lpKeyName, char* lpDefault, char* lpReturnedString, DWORD nSize, char* lpFileName);
	static DWORD WinWritePrivateProfileString (char* lpAppName, char* lpKeyName, char* lpString, char* lpFileName);

	static void DebugOut (char* str);
	static void DebugBreak();
	static float GetTime();

	static char* GetFocusWindow();

	static void WriteToDebugFile (char *strText);

	static char GetCdRomDriveWithGame();

	static void TimedKeyWait(int nKey1, int nKey2, int nKey3, float fWaitTime);
	static void RemoveMessages(int nMsg, int nMax = 100);
	static void RemoveKeyDownMessages(int nMax = 100);
	static void Sleep(DWORD dwSleep);
};

#endif
