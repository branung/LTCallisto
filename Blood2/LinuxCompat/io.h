// Linux stand-in for MSVC's <io.h>: _open and _close to test that a save exists,
// and _findfirst, _findnext and _findclose to list the .b2c character files, done with glob(3)
#ifndef BLOOD2_LINUXCOMPAT_IO_H
#define BLOOD2_LINUXCOMPAT_IO_H
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <string.h>
#include <stdlib.h>
#ifndef _O_RDONLY
#define _O_RDONLY O_RDONLY
#endif
inline int _open(const char *p, int f) { return open(p, f); }
// BloodServerShell's save test is `if (int fhl = _open(...) != -1) _close(fhl)`, which closes fd 1 when the save exists.
// On Windows that went unnoticed. Here fd 1 is stdout, so 0 to 2 are never closed. The real descriptor leaks, as it did in 1998
inline int _close(int fd) { return fd > 2 ? close(fd) : 0; }

struct _finddata_t { unsigned attrib; long size; char name[260]; };
struct b2_find { glob_t g; size_t next; };
inline long b2_find_fill(b2_find *f, struct _finddata_t *d)
{
	if (f->next >= f->g.gl_pathc) return -1;
	const char *p = f->g.gl_pathv[f->next++];
	strncpy(d->name, p, sizeof(d->name) - 1); d->name[sizeof(d->name) - 1] = 0;
	d->attrib = 0; d->size = 0;
	return 0;
}
inline long _findfirst(const char *pattern, struct _finddata_t *d)
{
	b2_find *f = new b2_find;
	f->next = 0;
	if (glob(pattern, 0, NULL, &f->g) != 0 || b2_find_fill(f, d) != 0) { globfree(&f->g); delete f; return -1; }
	return (long)(intptr_t)f;
}
inline int _findnext(long h, struct _finddata_t *d) { return h == -1 ? -1 : (int)b2_find_fill((b2_find *)(intptr_t)h, d); }
inline int _findclose(long h) { if (h != -1) { b2_find *f = (b2_find *)(intptr_t)h; globfree(&f->g); delete f; } return 0; }
#endif
