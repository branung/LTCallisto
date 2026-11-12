#!/usr/bin/env python3
"""Check, pack and start a mod for Shogo or Blood II.

    modtool.py new   <dir> --game Shogo|Blood2 --id ID [--name NAME]
    modtool.py check <dir|mod.rez> [--build DIR]
    modtool.py pack  <dir> <out.rez> [--build DIR]
    modtool.py --selftest

NEW writes <dir>/Mod/Mod.txt and nothing else, so a mod starts as the game
does: every table is retail until the mod copies a section into it.

CHECK says whether the game will take the mod, without starting it:
  - the manifest, by the game's own rules (Id, Game);
  - every table under Mod\\, and its Script.lua and ClientScript.lua, through
    the game's own loaders (testShogoTables or testBlood2Tables --load-mod,
    built from the code the shells run), so a file check accepts is a file the
    game accepts, and an error is the line the game's console would print;
  - any file under Mod\\ that no table of this game reads, since a typo in a
    file name is otherwise silent;
  - the placeholders Mod\\Placeholders.txt lists;
  - the signature a server and its clients compare (tools/lt1mod_sig.py).
A .rez is extracted first and checked the same way. Exit 0 when the game
would load every table; 1 when it would refuse one; 2 for a usage error.

PACK builds <out.rez> with LithRez's "l" option (without it LithRez leaves out
a lower case Mod\\ directory), extracts it again, and holds the archive's
signature against the directory's: an archive that signs differently is not
the mod the directory is, and pack fails.

Neither needs the game's data. CHECK needs the table test for the mod's game
built (BUILD_TESTS with BUILD_SHOGO or BUILD_BLOOD2); PACK and checking a .rez
need LithRez (BUILD_TOOLS). --build names the build tree; by default the first
of the usual ones that has the binary.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import lt1mod_sig as SIG                                    # noqa: E402

# The files each game reads under Mod\, and the two every mod may carry
TABLES = {
    'shogo': ['Factions.txt', 'Weapons.txt', 'Models.txt', 'Characters.txt', 'PlayerModes.txt',
              'Surfaces.txt', 'Strings.txt', 'Classes.txt', 'Commands.txt', 'Script.lua', 'ClientScript.lua'],
    'blood2': ['Weapons.txt', 'Characters.txt', 'AI.txt', 'Commands.txt', 'Strings.txt', 'Script.lua',
               'ClientScript.lua'],
}
ALWAYS = ['Mod.txt', 'Placeholders.txt']
TESTER = {'shogo': 'testShogoTables', 'blood2': 'testBlood2Tables'}
GAMES = {'shogo': 'Shogo', 'blood2': 'Blood2'}
BUILDS = ['build_dxvk', 'build_linux', 'build']


def find_binary(name, build=None):
    trees = [build] if build else [os.path.join(ROOT, b) for b in BUILDS]
    for t in trees:
        for cfg in ['', 'Release', 'RelWithDebInfo', 'Debug', 'MinSizeRel']:
            for ext in ['', '.exe']:
                p = os.path.join(t, 'OUT', cfg, name + ext)
                if os.path.isfile(p):
                    return p
    return None


def mod_dir(root):
    """The directory under root named Mod in any case, or None."""
    for n in os.listdir(root):
        if n.lower() == 'mod' and os.path.isdir(os.path.join(root, n)):
            return os.path.join(root, n)
    return None


def manifest_game(text):
    m = re.search(r'(?im)^\s*Game\s*=\s*"([^"]*)"', text)
    return m.group(1) if m else None


def placeholders(mdir):
    out = []
    for n in os.listdir(mdir):
        if n.lower() == 'placeholders.txt':
            with open(os.path.join(mdir, n), encoding='latin-1') as f:
                for line in f:
                    m = re.match(r'\s*PLACEHOLDER\s+(\S+)\s*(.*)', line)
                    if m:
                        out.append((m.group(1), m.group(2).strip()))
    return out


def extract(lithrez, rez, dest):
    subprocess.run([lithrez, 'x', os.path.abspath(rez), dest], stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)


def check(path, build=None, out=print):
    tmp = None
    if not os.path.exists(path):
        out('error: %s does not exist' % path)
        return 2
    try:
        if os.path.isfile(path):
            lithrez = find_binary('LithRez', build)
            if not lithrez:
                out('error: no LithRez built to open %s' % path)
                return 2
            tmp = tempfile.mkdtemp()
            extract(lithrez, path, tmp)
            root = tmp
        else:
            root = path
        mdir = mod_dir(root)
        if not mdir:
            out('error: %s has no Mod\\ directory, so it is not a mod' % path)
            return 1
        man = [n for n in os.listdir(mdir) if n.lower() == 'mod.txt']
        if not man:
            out('error: no Mod\\Mod.txt: the game would run unmodded')
            return 1
        with open(os.path.join(mdir, man[0]), encoding='latin-1') as f:
            game = manifest_game(f.read())
        key = (game or '').lower()
        if key not in TABLES:
            out('error: Mod\\Mod.txt names Game %s; it must be "Shogo" or "Blood2"' % (
                '"%s"' % game if game is not None else 'nothing'))
            return 1

        rc = 0
        tester = find_binary(TESTER[key], build)
        if not tester:
            out('error: %s is not built, so the tables cannot be checked (BUILD_%s)' % (
                TESTER[key], key.upper()))
            rc = 2
        else:
            # The game finds a file in any case, the test's loaders by its exact name, and
            # LithRez extracts Mod.TXT; so they get a copy spelled as the tables spell their files
            run_root = tempfile.mkdtemp()
            canon = {n.lower(): n for n in TABLES[key] + ALWAYS}
            os.makedirs(os.path.join(run_root, 'Mod'))
            for n in os.listdir(mdir):
                if os.path.isfile(os.path.join(mdir, n)):
                    shutil.copyfile(os.path.join(mdir, n), os.path.join(run_root, 'Mod', canon.get(n.lower(), n)))
            try:
                r = subprocess.run([tester, '--load-mod', run_root], capture_output=True, text=True,
                                   errors='replace')
            finally:
                shutil.rmtree(run_root, ignore_errors=True)
            nwarn = 0
            for line in (r.stdout + r.stderr).splitlines():
                out('  ' + line)
                nwarn += 'WARNING' in line
            if r.returncode != 0:
                out('error: the game would refuse this mod, as above')
                rc = 1

        known = {n.lower() for n in TABLES[key] + ALWAYS}
        for dp, _dn, fn in os.walk(mdir):
            for n in sorted(fn):
                rel = os.path.relpath(os.path.join(dp, n), mdir)
                if os.sep in rel or n.lower() not in known:
                    out('warning: Mod\\%s is not a file %s reads; it is signed, but nothing opens it'
                        % (rel.replace(os.sep, '\\'), GAMES[key]))
        ph = placeholders(mdir)
        if ph:
            out('%d placeholder%s (Mod\\Placeholders.txt):' % (len(ph), '' if len(ph) == 1 else 's'))
            for p, what in ph:
                out('  PLACEHOLDER  %s  %s' % (p, what))
        sig, _h, n = SIG.signature(SIG.dir_files(root))
        out('signature %s (%d file%s under Mod\\)' % (sig, n, '' if n == 1 else 's'))
        if rc == 0:
            # A script warning does not stop the game, but it is a call or an event the author
            # did not mean
            out('ok: %s would load this mod%s' % (GAMES[key], ', with %d script warning%s above' % (
                nwarn, '' if nwarn == 1 else 's') if tester and nwarn else ''))
        return rc
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)


def pack(src, out_rez, build=None, out=print):
    lithrez = find_binary('LithRez', build)
    if not lithrez:
        out('error: no LithRez built')
        return 2
    if not mod_dir(src):
        out('error: %s has no Mod\\ directory' % src)
        return 1
    src = os.path.abspath(src)
    out_rez = os.path.abspath(out_rez)
    if os.path.exists(out_rez):
        os.remove(out_rez)
    parent, leaf = os.path.split(src)
    subprocess.run([lithrez, 'cl', out_rez, leaf, '*.*'], cwd=parent, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=False)
    if not os.path.isfile(out_rez) or os.path.getsize(out_rez) == 0:
        out('error: LithRez wrote nothing')
        return 1
    want = SIG.signature(SIG.dir_files(src))[0]
    tmp = tempfile.mkdtemp()
    try:
        extract(lithrez, out_rez, tmp)
        got = SIG.signature(SIG.dir_files(tmp))[0] if mod_dir(tmp) else 'nothing'
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if want != got:
        out('error: %s signs %s but the directory signs %s: the archive is not the mod' % (out_rez, got, want))
        return 1
    out('%s: signature %s, the same as the directory\'s' % (out_rez, got))
    return 0


def new(dest, game, mod_id, name=None, out=print):
    if game.lower() not in GAMES:
        out('error: --game is Shogo or Blood2')
        return 2
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,31}', mod_id):
        out('error: an Id is letters, digits, _ and -, at most 31')
        return 2
    path = os.path.join(dest, 'Mod', 'Mod.txt')
    if os.path.exists(path):
        out('error: %s exists' % path)
        return 1
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='\r\n') as f:
        f.write('[Mod]\nId      = "%s"\nGame    = "%s"\nName    = "%s"\nVersion = "1"\n'
                % (mod_id, GAMES[game.lower()], name or mod_id))
    ref = 'Shogo' if game.lower() == 'shogo' else 'Blood2'
    out('%s: copy the sections you change from %s/ModReference/ into %s'
        % (path, ref, os.path.join(dest, 'Mod')))
    return 0


def selftest():
    ok = True

    def expect(cond, what):
        nonlocal ok
        print('  %s  %s' % ('ok  ' if cond else 'FAIL', what))
        ok = ok and cond

    with tempfile.TemporaryDirectory() as t:
        lines = []
        expect(new(os.path.join(t, 'm'), 'Shogo', 'T', out=lines.append) == 0 and
               os.path.isfile(os.path.join(t, 'm', 'Mod', 'Mod.txt')), 'new writes a manifest')
        expect(new(os.path.join(t, 'm'), 'Shogo', 'T', out=lines.append) == 1, 'and never over one')
        expect(new(os.path.join(t, 'n'), 'Quake', 'T', out=lines.append) == 2 and
               new(os.path.join(t, 'n'), 'Shogo', 'bad id', out=lines.append) == 2,
               'an unknown game or a bad Id is a usage error')
        lines = []
        os.makedirs(os.path.join(t, 'x'))
        expect(check(os.path.join(t, 'x'), out=lines.append) == 1 and 'no Mod' in lines[-1],
               'a tree without Mod\\ is not a mod')
        with open(os.path.join(t, 'm', 'Mod', 'Mod.txt'), 'w') as f:
            f.write('[Mod]\nId = "T"\nGame = "Quake"\n')
        lines = []
        expect(check(os.path.join(t, 'm'), out=lines.append) == 1 and 'Quake' in lines[-1],
               'a manifest for another game is refused before any table')
        os.makedirs(os.path.join(t, 'p', 'Mod'))
        with open(os.path.join(t, 'p', 'Mod', 'Placeholders.txt'), 'w') as f:
            f.write('// x\nPLACEHOLDER  Skins\\a.dtx  flat magenta\nnot one\n')
        expect(placeholders(os.path.join(t, 'p', 'Mod')) == [('Skins\\a.dtx', 'flat magenta')],
               'placeholders are the PLACEHOLDER lines')
    print('passed' if ok else 'FAILED')
    return 0 if ok else 1


def main(argv):
    args = argv[1:]
    build = None
    if '--build' in args:
        i = args.index('--build')
        build = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    if args == ['--selftest']:
        return selftest()
    if len(args) >= 2 and args[0] == 'check':
        return check(args[1], build)
    if len(args) == 3 and args[0] == 'pack':
        return pack(args[1], args[2], build)
    if len(args) >= 2 and args[0] == 'new':
        rest = args[2:]
        opts = dict(zip(rest[0::2], rest[1::2]))
        if '--game' in opts and '--id' in opts:
            return new(args[1], opts['--game'], opts['--id'], opts.get('--name'))
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    # Plain \n even on Windows, where a redirected stdout would otherwise write \r\n and a
    # $ anchored grep of this output would not match under Git Bash
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(newline='\n')
    sys.exit(main(sys.argv))
