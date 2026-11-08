// Linux versions of the Win32 calls Blood 2's client shell makes, included at the end of windows.h.
//
// Real ones, because the game depends on them: the clock, the save game's file calls (LoadSave.cpp)
// and WinUtil's existence tests. Paths are Windows paths: '\' becomes '/', each existing component
// is matched without case as Windows does, and "*.*" means every file.
//
// Inert ones, since Linux has no Win32 window: the dialog, window, message and resource calls.
// A dialog that can't be shown is cancelled (IDCANCEL, 2). NetStart's wizard checks for 2 and 4,
// so Win32's own failure value, -1, would walk it to StartGame with nothing filled in.
// The rest answer "nothing there", and MessageBox prints to stderr
#ifndef BLOOD2_LINUXCOMPAT_WIN32_LINUX_H
#define BLOOD2_LINUXCOMPAT_WIN32_LINUX_H
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string>

// --- paths ------------------------------------------------------------------

// A Windows path as this file system needs it: '\' to '/', and each existing component matched without case.
// From the first component that doesn't exist on, it's kept as written, for a file or folder about to be created
inline std::string b2_path(const char *p)
{
	std::string s(p ? p : "");
	for (size_t i = 0; i < s.size(); ++i)
		if (s[i] == '\\') s[i] = '/';
	std::string out = (!s.empty() && s[0] == '/') ? "/" : "";
	size_t i = out.size();
	bool exists = true;
	while (i <= s.size())
	{
		size_t j = s.find('/', i);
		if (j == std::string::npos) j = s.size();
		std::string part = s.substr(i, j - i);
		if (!part.empty())
		{
			std::string dir = out.empty() ? "." : out;
			std::string hit = part;
			if (exists && part != "." && part != ".." && part.find_first_of("*?") == std::string::npos)
			{
				struct stat st;
				std::string exact = (out.empty() ? "" : out) + part;
				if (stat(exact.c_str(), &st) != 0)
				{
					hit.clear();
					if (DIR *d = opendir(dir.c_str()))
					{
						while (struct dirent *e = readdir(d))
							if (strcasecmp(e->d_name, part.c_str()) == 0) { hit = e->d_name; break; }
						closedir(d);
					}
					if (hit.empty()) { hit = part; exists = false; }
				}
			}
			out += hit;
		}
		if (j < s.size()) out += '/';
		i = j + 1;
	}
	return out;
}

// --- the clock ----------------------------------------------------------------

inline DWORD GetTickCount()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}
inline void Sleep(DWORD ms) { usleep((useconds_t)ms * 1000u); }

// --- files --------------------------------------------------------------------

inline DWORD GetLastError() { return (DWORD)errno; }

inline HANDLE CreateFile(LPCSTR name, DWORD access, DWORD, void *, DWORD disposition, DWORD, HANDLE)
{
	int flags = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
	if (disposition == CREATE_ALWAYS) flags |= O_CREAT | O_TRUNC;
	else if (disposition == CREATE_NEW) flags |= O_CREAT | O_EXCL;
	else if (disposition == OPEN_ALWAYS) flags |= O_CREAT;
	int fd = open(b2_path(name).c_str(), flags, 0644);
	return fd < 0 ? INVALID_HANDLE_VALUE : (HANDLE)(intptr_t)fd;
}
inline BOOL ReadFile(HANDLE h, void *buf, DWORD n, DWORD *got, void *)
{
	ssize_t r = read((int)(intptr_t)h, buf, n);
	if (got) *got = r < 0 ? 0 : (DWORD)r;
	return r < 0 ? FALSE : TRUE;
}
inline BOOL WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *put, void *)
{
	ssize_t r = write((int)(intptr_t)h, buf, n);
	if (put) *put = r < 0 ? 0 : (DWORD)r;
	return r == (ssize_t)n ? TRUE : FALSE;
}
inline BOOL CloseHandle(HANDLE h) { return close((int)(intptr_t)h) == 0 ? TRUE : FALSE; }
inline BOOL DeleteFile(LPCSTR name) { return unlink(b2_path(name).c_str()) == 0 ? TRUE : FALSE; }
inline BOOL CreateDirectory(LPCSTR name, void *) { return mkdir(b2_path(name).c_str(), 0755) == 0 ? TRUE : FALSE; }
inline BOOL CopyFile(LPCSTR from, LPCSTR to, BOOL failIfExists)
{
	std::string t = b2_path(to);
	if (failIfExists && access(t.c_str(), F_OK) == 0) { errno = EEXIST; return FALSE; }
	FILE *in = fopen(b2_path(from).c_str(), "rb");
	if (!in) return FALSE;
	FILE *out = fopen(t.c_str(), "wb");
	if (!out) { fclose(in); return FALSE; }
	char buf[65536];
	size_t n;
	bool ok = true;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
	fclose(in);
	if (fclose(out) != 0) ok = false;
	return ok ? TRUE : FALSE;
}

// OpenFile is only used as an existence test (WinUtil::FileExist, OF_EXIST)
inline HFILE OpenFile(LPCSTR name, OFSTRUCT *, UINT)
{
	return access(b2_path(name).c_str(), F_OK) == 0 ? (HFILE)1 : HFILE_ERROR;
}

struct b2_find_handle { glob_t g; size_t next; };

inline BOOL b2_find_fill(b2_find_handle *f, WIN32_FIND_DATA *d)
{
	if (f->next >= f->g.gl_pathc) return FALSE;
	const char *p = f->g.gl_pathv[f->next++];
	const char *base = strrchr(p, '/');
	base = base ? base + 1 : p;
	memset(d, 0, sizeof(*d));
	strncpy(d->cFileName, base, sizeof(d->cFileName) - 1);
	struct stat st;
	if (stat(p, &st) == 0)
	{
		d->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
		if (!(st.st_mode & S_IWUSR)) d->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
		d->nFileSizeLow = (DWORD)st.st_size;
	}
	return TRUE;
}
inline HANDLE FindFirstFile(LPCSTR pattern, WIN32_FIND_DATA *d)
{
	std::string p = b2_path(pattern);
	if (p.size() >= 3 && p.compare(p.size() - 3, 3, "*.*") == 0)
		p.replace(p.size() - 3, 3, "*");          // every file, as on Windows
	if (!p.empty() && p[p.size() - 1] == '/')
		p.erase(p.size() - 1);                    // "Save\" names the directory itself
	b2_find_handle *f = new b2_find_handle;
	f->next = 0;
	if (glob(p.c_str(), 0, NULL, &f->g) != 0 || !b2_find_fill(f, d))
	{
		globfree(&f->g);
		delete f;
		errno = ENOENT;
		return INVALID_HANDLE_VALUE;
	}
	return (HANDLE)f;
}
// Both refuse a handle FindFirstFile didn't return, as Win32 does.
// LoadSave.cpp closes every handle right after opening it, failed or not
inline BOOL b2_find_valid(HANDLE h) { return h && h != INVALID_HANDLE_VALUE; }
inline BOOL FindNextFile(HANDLE h, WIN32_FIND_DATA *d)
{
	return b2_find_valid(h) && b2_find_fill((b2_find_handle *)h, d);
}
inline BOOL FindClose(HANDLE h)
{
	if (!b2_find_valid(h))
		return FALSE;
	b2_find_handle *f = (b2_find_handle *)h;
	globfree(&f->g);
	delete f;
	return TRUE;
}

// No drive letters here, so no CD-ROM. WinUtil's two CD checks find none and fall back,
// as they do on a machine without the disc
inline UINT GetDriveType(LPCSTR) { return DRIVE_NO_ROOT_DIR; }
inline UINT SetErrorMode(UINT) { return 0; }

// The INI calls have no caller outside WinUtil's own wrappers.
// They answer the default and write nothing
inline DWORD GetPrivateProfileString(LPCSTR, LPCSTR, LPCSTR def, LPSTR out, DWORD n, LPCSTR)
{
	if (!out || !n) return 0;
	strncpy(out, def ? def : "", n - 1);
	out[n - 1] = 0;
	return (DWORD)strlen(out);
}
inline BOOL WritePrivateProfileString(LPCSTR, LPCSTR, LPCSTR, LPCSTR) { return FALSE; }

inline void OutputDebugString(LPCSTR s) { if (s) fputs(s, stderr); }
inline void DebugBreak() {}

// --- inert: windows, dialogs, messages, resources, the keyboard --------------

inline INT_PTR DialogBox(HINSTANCE, LPCSTR, HWND, DLGPROC) { return IDCANCEL; }
inline BOOL EndDialog(HWND, INT_PTR) { return TRUE; }
inline LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline HWND GetDlgItem(HWND, int) { return NULL; }
inline UINT IsDlgButtonChecked(HWND, int) { return BST_UNCHECKED; }
inline BOOL CheckDlgButton(HWND, int, UINT) { return TRUE; }
inline UINT GetDlgItemInt(HWND, int, BOOL *ok, BOOL) { if (ok) *ok = FALSE; return 0; }
inline BOOL SetDlgItemInt(HWND, int, UINT, BOOL) { return TRUE; }
inline UINT GetDlgItemText(HWND, int, LPSTR s, int n) { if (s && n > 0) s[0] = 0; return 0; }
inline BOOL SetDlgItemText(HWND, int, LPCSTR) { return TRUE; }
inline int GetWindowText(HWND, LPSTR s, int n) { if (s && n > 0) s[0] = 0; return 0; }
inline BOOL SetWindowText(HWND, LPCSTR) { return TRUE; }
inline BOOL ShowWindow(HWND, int) { return FALSE; }
inline BOOL UpdateWindow(HWND) { return TRUE; }
inline BOOL InvalidateRect(HWND, const void *, BOOL) { return TRUE; }
inline BOOL EnableWindow(HWND, BOOL) { return FALSE; }
inline HWND SetFocus(HWND) { return NULL; }
inline HWND GetFocus() { return NULL; }
inline HWND GetActiveWindow() { return NULL; }
inline HWND SetActiveWindow(HWND) { return NULL; }
inline HWND GetForegroundWindow() { return NULL; }
inline BOOL SetForegroundWindow(HWND) { return FALSE; }
inline BOOL BringWindowToTop(HWND) { return FALSE; }
inline int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT)
{
	fprintf(stderr, "Blood 2: %s: %s\n", caption ? caption : "", text ? text : "");
	return IDOK;
}
inline int LoadString(HINSTANCE, UINT, LPSTR s, int n) { if (s && n > 0) s[0] = 0; return 0; }
inline BOOL PeekMessage(MSG *, HWND, UINT, UINT, UINT) { return FALSE; }
inline BOOL PostMessage(HWND, UINT, WPARAM, LPARAM) { return TRUE; }
inline uintptr_t SetTimer(HWND, uintptr_t id, UINT, void *) { return id; }
inline BOOL KillTimer(HWND, uintptr_t) { return TRUE; }
inline HCURSOR LoadCursor(HINSTANCE, LPCSTR) { return NULL; }
inline HCURSOR SetCursor(HCURSOR) { return NULL; }
inline uintptr_t GetClassLongPtr(HWND, int) { return 0; }
inline SHORT GetAsyncKeyState(int) { return 0; }
inline HKL GetKeyboardLayout(DWORD) { return NULL; }
inline UINT MapVirtualKeyEx(UINT, UINT, HKL) { return 0; }
inline HRSRC FindResource(HMODULE, LPCSTR, LPCSTR) { return NULL; }
inline HGLOBAL LoadResource(HMODULE, HRSRC) { return NULL; }
inline void *LockResource(HGLOBAL) { return NULL; }

#endif
