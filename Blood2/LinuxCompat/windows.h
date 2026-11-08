// Linux stand-in for <windows.h>: the Win32 types and constants Blood 2's game code uses.
// Each constant is guarded so a definition the game makes itself wins.
// The calls themselves are in win32_linux.h, included at the end
#ifndef BLOOD2_LINUXCOMPAT_WINDOWS_H
#define BLOOD2_LINUXCOMPAT_WINDOWS_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t DWORD;
// WinUtil.h typedefs DWORD as unsigned long unless DWORD is a macro.
// unsigned long is 64 bits on Linux, so this keeps the 32-bit one and tells WinUtil.h it's there
#define DWORD DWORD
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef int BOOL;
// The same for Misc/StdLithDefs.h's #ifndef fallbacks
#define WORD WORD
#define BYTE BYTE
#define BOOL BOOL
typedef long LONG;
typedef short SHORT;
typedef unsigned int UINT;
typedef void *HANDLE;
typedef void *HWND;
typedef void *HINSTANCE;
typedef void *HMODULE;
typedef void *HKL;
typedef void *HRSRC;
typedef void *HGLOBAL;
typedef int HFILE;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef DWORD *LPDWORD;
typedef void *LPVOID;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef intptr_t INT_PTR;
typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
// x86 Win32 calling conventions. There's only one convention here
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#ifndef HFILE_ERROR
#define HFILE_ERROR ((HFILE)-1)
#endif

typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; } MSG;
typedef struct _OFSTRUCT { BYTE cBytes; BYTE fFixedDisk; WORD nErrCode; WORD Reserved1, Reserved2; char szPathName[128]; } OFSTRUCT;
typedef struct _WIN32_FIND_DATA
{
	DWORD dwFileAttributes;
	DWORD nFileSizeHigh, nFileSizeLow;
	char cFileName[MAX_PATH];
	char cAlternateFileName[14];
} WIN32_FIND_DATA;

// files
#ifndef GENERIC_READ
#define GENERIC_READ  0x80000000u
#define GENERIC_WRITE 0x40000000u
#endif
#ifndef CREATE_NEW
#define CREATE_NEW    1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS   4
#endif
#ifndef FILE_ATTRIBUTE_READONLY
#define FILE_ATTRIBUTE_READONLY  0x01
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL    0x80
#endif
#ifndef FILE_SHARE_READ
#define FILE_SHARE_READ  1
#define FILE_SHARE_WRITE 2
#endif
#ifndef OF_EXIST
#define OF_EXIST 0x4000
#endif
#ifndef DRIVE_NO_ROOT_DIR
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_CDROM       5
#endif
#ifndef SEM_FAILCRITICALERRORS
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOOPENFILEERRORBOX 0x8000
#endif

// dialogs, windows and messages
#ifndef IDOK
#define IDOK     1
#define IDCANCEL 2
#endif
#ifndef MB_OK
#define MB_OK              0x00
#define MB_ICONINFORMATION 0x40
#endif
#ifndef SW_HIDE
#define SW_HIDE 0
#define SW_SHOW 5
#endif
#ifndef PM_REMOVE
#define PM_REMOVE 1
#endif
#ifndef WM_SETREDRAW
#define WM_SETREDRAW   0x000B
#define WM_ACTIVATE    0x0006
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR   0x0020
#define WM_KEYDOWN     0x0100
#define WM_INITDIALOG  0x0110
#define WM_COMMAND     0x0111
#define WM_TIMER       0x0113
#endif
#ifndef SW_NORMAL
#define SW_NORMAL 1
#endif
#ifndef LBN_SELCHANGE
#define LBN_SELCHANGE 1
#define LBN_DBLCLK    2
#endif
#ifndef LB_SETTABSTOPS
#define LB_SETTABSTOPS 0x0192
#endif
#ifndef LOWORD
#define LOWORD(l) ((WORD)((uintptr_t)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((uintptr_t)(l) >> 16) & 0xffff))
#endif
typedef void *HCURSOR;
#ifndef GCLP_HCURSOR
#define GCLP_HCURSOR (-12)
#endif
#ifndef IDC_WAIT
#define IDC_WAIT ((LPCSTR)(intptr_t)32514)
#endif
#ifndef VK_SHIFT
#define VK_SHIFT 16      // VKDefs.h's spelling, so the two agree token for token
#endif
#ifndef BST_UNCHECKED
#define BST_UNCHECKED 0
#define BST_CHECKED   1
#endif
#ifndef LB_ERR
#define LB_ERR          (-1)
#define LB_ADDSTRING    0x0180
#define LB_DELETESTRING 0x0182
#define LB_RESETCONTENT 0x0184
#define LB_SETCURSEL    0x0186
#define LB_GETCURSEL    0x0188
#define LB_GETTEXT      0x0189
#define LB_GETCOUNT     0x018B
#define LB_GETITEMDATA  0x0199
#define LB_SETITEMDATA  0x019A
#endif
#ifndef CB_ERR
#define CB_ERR          (-1)
#define CB_ADDSTRING    0x0143
#define CB_GETCOUNT     0x0146
#define CB_GETCURSEL    0x0147
#define CB_GETLBTEXT    0x0148
#define CB_RESETCONTENT 0x014B
#define CB_SETCURSEL    0x014E
#define CB_GETITEMDATA  0x0150
#define CB_SETITEMDATA  0x0151
#endif

// MSVC's CRT spelling of sprintf for the window API's wsprintf
#ifndef wsprintf
#define wsprintf sprintf
#endif

// _mbstrlen, _mbscpy and _mbsncpy come from the engine's own single-byte definitions,
// since a second copy would collide with them in a file that includes both
#include "ltbasedefs.h"

#include "win32_linux.h"
#endif
