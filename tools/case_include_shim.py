#!/usr/bin/env python3
"""case_include_shim.py OUT SRCDIR... -I INCDIR...: lets 1998 Windows game code's
#include names resolve on a case-sensitive file system, without editing the code.

MSVC found `#include "DLink.h"` as dlink.h and read backslashes in a path as separators.
For each include under SRCDIR that doesn't resolve exactly (the including file's folder first,
then the -I folders in order, as GCC searches) but does with case folded, this makes
OUT/<name as written> a symbolic link to the file found. OUT goes on the include path after the
real folders, so an include that already resolves never reaches it.

A name that would need two different targets is reported and left out.
A system header's exact name is never linked, and one in MSVC's spelling ("Assert.h")
links to the system header. Run by Blood 2's CMake files on Linux when configuring."""
import os, re, sys

INC = re.compile(r'^\s*#\s*include\s*["<]([^">]+)[">]', re.M)

def resolve_ci(name, base, dirs):
    """name as written, from a file in base: the file it means, or None."""
    parts = name.replace('\\', '/').split('/')
    for d in [base] + dirs:
        cur = d
        for i, part in enumerate(parts):
            if part in ('.', '..'):
                cur = os.path.normpath(os.path.join(cur, part)); continue
            try:
                ents = os.listdir(cur)
            except OSError:
                cur = None; break
            hit = [e for e in ents if e.lower() == part.lower()]
            if not hit:
                cur = None; break
            cur = os.path.join(cur, sorted(hit)[0])
        if cur and os.path.isfile(cur):
            return cur
    return None

def system_dirs():
    """The compiler's own search list: a name found there is the C or C++
    library's (<stdio.h>, "memory.h") and must never be linked to a game file
    or stand-in that merely shares its folded spelling."""
    import subprocess
    try:
        err = subprocess.run(['c++', '-xc++', '-E', '-v', '-'], input='', capture_output=True,
                             text=True).stderr
    except OSError:
        return ['/usr/include', '/usr/local/include']
    lines = err.splitlines(); out = []; on = False
    for l in lines:
        if l.startswith('#include <...> search starts here'): on = True; continue
        if l.startswith('End of search list'): break
        if on: out.append(l.strip())
    return out or ['/usr/include']

def main(argv):
    if '-I' not in argv or len(argv) < 3:
        print(__doc__); return 2
    out = os.path.abspath(argv[0]); k = argv.index('-I')
    srcs = [os.path.abspath(a) for a in argv[1:k]]
    incs = [os.path.abspath(a) for a in argv[k + 1:]]
    os.makedirs(out, exist_ok=True)
    sysdirs = system_dirs()
    want = {}
    for s in srcs:
        for root, _, files in os.walk(s):
            for f in files:
                if not f.lower().endswith(('.cpp', '.h', '.c')):
                    continue
                p = os.path.join(root, f)
                try:
                    text = open(p, encoding='latin-1').read()
                except OSError:
                    continue
                for name in INC.findall(text):
                    exact = [os.path.join(d, name) for d in [root] + incs]
                    if '\\' not in name and any(os.path.isfile(e) for e in exact):
                        continue
                    if any(os.path.isfile(os.path.join(d, name)) for d in sysdirs):
                        continue
                    tgt = resolve_ci(name, root, incs)
                    if not tgt and not name.startswith('.'):
                        # "Assert.h", "sys\\stat.h": a system header's name in
                        # MSVC's spelling links to the system header itself
                        tgt = resolve_ci(name, sysdirs[0] if sysdirs else '/usr/include', sysdirs)
                    if tgt:
                        want.setdefault(name, set()).add(tgt)
    made = 0
    for name, tgts in sorted(want.items()):
        if len(tgts) > 1:
            print('ambiguous, left out: %s -> %s' % (name, sorted(tgts))); continue
        link = os.path.join(out, name)          # a backslash stays in the file name
        os.makedirs(os.path.dirname(link), exist_ok=True)
        tgt = next(iter(tgts))
        if os.path.islink(link) and os.readlink(link) == tgt:
            continue
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(tgt, link); made += 1
    print('case_include_shim: %d names, %d links made in %s' % (len(want), made, out))
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
