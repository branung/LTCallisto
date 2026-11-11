#!/usr/bin/env python3
"""The connect signature of a Shogo or Blood II mod, computed outside the game.

The game hashes every file under Mod\\ of the mounted trees, and a server and a
client compare the result before playing together. This computes the same value
from the files, so a mod author can see the signature a build will carry and two
players can compare theirs without starting the game. It is written from the
definition below, not from lt1mod.cpp, so the two agreeing is evidence the C++
hashes what it claims to.

THE HASH. FNV-1a, 64-bit (offset basis 14695981039346656037, prime
1099511628211), over every file under Mod\\, sorted by its path lower-cased and
'\\'-separated (`mod\\weapons.txt`). Per file: that path, one zero byte, the
file's length as eight little-endian bytes, the file's bytes. The signature is
`<Id>:<16 hex digits>`, `none` with no Mod\\Mod.txt, and `invalid:<hex>` when
the manifest names no usable Id. The Id is read with a pattern, not a full
parse, so the game is the authority on whether the rest of the manifest is valid.

USAGE
    lt1mod_sig.py <folder> [<folder> ...]   mod folders in mount order; a later
                                            one overrides an earlier one per file
    lt1mod_sig.py --selftest
"""
import os
import re
import sys

FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK = 0xffffffffffffffff


def fnv1a64(chunks):
    h = FNV_OFFSET
    for data in chunks:
        for b in data:
            h ^= b
            h = (h * FNV_PRIME) & MASK
    return h


def norm(path):
    return path.replace('/', '\\').lower()


def dir_files(root):
    """{normalized path: bytes} for every file under root/Mod, any case."""
    out = {}
    for name in os.listdir(root):
        if name.lower() == 'mod' and os.path.isdir(os.path.join(root, name)):
            base = os.path.join(root, name)
            for dp, _dn, fn in os.walk(base):
                for f in fn:
                    full = os.path.join(dp, f)
                    rel = os.path.relpath(full, root)
                    with open(full, 'rb') as fh:
                        out[norm(rel)] = fh.read()
    return out


def mounted(trees):
    files = {}
    for t in trees:
        if not os.path.isdir(t):
            raise SystemExit('%s: not a folder (give the folder the mod was packed from)' % t)
        files.update(dir_files(t))
    return files


def signature(files):
    if 'mod\\mod.txt' not in files:
        return 'none', 0, 0
    chunks = []
    for p in sorted(files):
        data = files[p]
        chunks += [p.encode('latin-1'), b'\0', len(data).to_bytes(8, 'little'), data]
    h = fnv1a64(chunks)
    m = re.search(rb'(?im)^\s*Id\s*=\s*"([A-Za-z0-9_-]{1,31})"', files['mod\\mod.txt'])
    sig = ('%s:%016x' % (m.group(1).decode(), h)) if m else ('invalid:%016x' % h)
    return sig, h, len(files)


def selftest():
    ok = True

    def check(cond, what):
        nonlocal ok
        print('  %s  %s' % ('ok  ' if cond else 'FAIL', what))
        ok = ok and cond

    # A known answer derived by hand from the definition: FNV-1a over "mod\\mod.txt", 0x00,
    # 30 as eight little-endian bytes, then the text
    text = b'[Mod]\nId = "K"\nGame = "Shogo"\n'
    sig, _h, n = signature({'mod\\mod.txt': text})
    check(sig == 'K:418d350ed64859dd' and n == 1, 'known answer %s' % sig)
    check(fnv1a64([b'']) == FNV_OFFSET, 'empty input is the offset basis')
    check(fnv1a64([b'a']) == 0xaf63dc4c8601ec8c, 'FNV-1a 64 of "a" (published vector)')
    check(signature({'worlds\\x.dat': b''})[0] == 'none', 'no manifest: none')
    a = signature({'mod\\mod.txt': text, 'mod\\a': b'xy', 'mod\\b': b''})[1]
    b = signature({'mod\\mod.txt': text, 'mod\\a': b'x', 'mod\\b': b'y'})[1]
    check(a != b, 'the length field separates files')
    check(signature({'mod\\mod.txt': b'[Mod]\nGame = "Shogo"\n'})[0].startswith('invalid:'),
          'no Id: invalid')
    print('passed' if ok else 'FAILED')
    return 0 if ok else 1


def main(argv):
    if argv[1:2] == ['--selftest']:
        return selftest()
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    sig, _h, n = signature(mounted(argv[1:]))
    print(sig)
    print('%d file%s under Mod\\' % (n, '' if n == 1 else 's'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
