#!/usr/bin/env python3
"""Check, pack and start a mod for Shogo or Blood II.

    modtool.py new   <dir> --game Shogo|Blood2 --id ID [--name NAME]
    modtool.py check <dir|mod.rez> [--addon] [--build DIR]
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

A Blood II mod is checked against the base game first. When the base game
refuses it, it is checked again against The Nightmare Levels'
(testBlood2TablesNM, built with BUILD_BLOOD2_ADDON), and if the add-on's
loaders take every table it is an add-on mod: "ok: Blood2NM would load this
mod", with the lines the base game refused. Both modules read the same
manifest, Game "Blood2", so the add-on's own tables are what tell the two
apart. --addon checks against the add-on alone, as the launcher's --addon
runs it.

PACK builds <out.rez> with LithRez's "l" option (without it LithRez leaves out
a lower case Mod\\ directory), extracts it again, and holds the archive's
signature against the directory's: an archive that signs differently is not
the mod the directory is, and pack fails.

Neither needs the game's data. CHECK needs the table test for the mod's game
built (BUILD_TESTS with BUILD_SHOGO or BUILD_BLOOD2, and BUILD_BLOOD2_ADDON as
well for a mod of The Nightmare Levels); PACK and checking a .rez need LithRez
(BUILD_TOOLS). --build names the build tree; by default the first of the usual
ones that has the binary.
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
    'blood2': ['Weapons.txt', 'Characters.txt', 'Ammo.txt', 'Projectiles.txt', 'WeaponClasses.txt', 'Rules.txt', 'Explosions.txt', 'Lasers.txt', 'ObjectFX.txt', 'Pickups.txt', 'AI.txt', 'Commands.txt', 'Strings.txt', 'Script.lua',
               'ClientScript.lua'],
}
ALWAYS = ['Mod.txt', 'Placeholders.txt']
TESTER = {'shogo': 'testShogoTables', 'blood2': 'testBlood2Tables'}
GAMES = {'shogo': 'Shogo', 'blood2': 'Blood2'}
# The Nightmare Levels: Blood II's modules built with _ADDON, which load the same Mod\Mod.txt
# (Game "Blood2") but know the add-on's weapons, characters and classes
ADDON_TESTER = {'blood2': 'testBlood2TablesNM'}
ADDON_GAMES = {'blood2': 'Blood2NM'}
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


def run_tables(tester, mdir, key):
    """The table test's --load-mod over mdir: (exit code, output lines)."""
    # The game finds a file in any case, the test's loaders by its exact name, and
    # LithRez extracts Mod.TXT; so they get a copy spelled as the tables spell their files
    run_root = tempfile.mkdtemp()
    canon = {n.lower(): n for n in TABLES[key] + ALWAYS}
    try:
        os.makedirs(os.path.join(run_root, 'Mod'))
        for n in os.listdir(mdir):
            if os.path.isfile(os.path.join(mdir, n)):
                shutil.copyfile(os.path.join(mdir, n), os.path.join(run_root, 'Mod', canon.get(n.lower(), n)))
            elif n.lower() == 'scripts':
                # The scripts' modules (API 7), which require reads as Mod\\Scripts\\<name>.lua. LithRez
                # extracts a packed module as <name>.LUA, so the extension is spelled again.
                for dp, _dn, fn in os.walk(os.path.join(mdir, n)):
                    dst = os.path.join(run_root, 'Mod', 'Scripts', os.path.relpath(dp, os.path.join(mdir, n)))
                    os.makedirs(dst, exist_ok=True)
                    for f in fn:
                        stem, ext = os.path.splitext(f)
                        shutil.copyfile(os.path.join(dp, f), os.path.join(dst, stem + ('.lua' if ext.lower() == '.lua' else ext)))
        r = RUN(tester, run_root)
    finally:
        shutil.rmtree(run_root, ignore_errors=True)
    return r


def _run(tester, run_root):
    r = subprocess.run([tester, '--load-mod', run_root], capture_output=True, text=True, errors='replace')
    return r.returncode, (r.stdout + r.stderr).splitlines()


RUN = _run          # the selftest stands a stub in for the table tests


def check(path, build=None, out=print, addon=False):
    """Exit code: 0 the game would load the mod, 1 it would not, 2 usage."""
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
        if addon and key not in ADDON_TESTER:
            out('error: --addon is for The Nightmare Levels; %s has no add-on' % GAMES[key])
            return 2
        tname, gname = (ADDON_TESTER[key], ADDON_GAMES[key]) if addon else (TESTER[key], GAMES[key])
        tester = find_binary(tname, build)
        lines = []
        if not tester:
            out('error: %s is not built, so the tables cannot be checked (BUILD_%s%s)' % (
                tname, key.upper(), '_ADDON' if addon else ''))
            rc = 2
        else:
            trc, lines = run_tables(tester, mdir, key)
            after = []      # printed below the tables' own lines
            if trc != 0 and not addon and key in ADDON_TESTER:
                # The base game refuses it. Both modules take Game "Blood2", so the add-on's own
                # loaders decide: if they take every table, everything the base game refused is
                # the add-on's, and the mod is an add-on mod.
                nm = find_binary(ADDON_TESTER[key], build)
                if not nm:
                    after = ['note: %s is not built (BUILD_BLOOD2_ADDON); a mod for The Nightmare Levels '
                             'is checked against it' % ADDON_TESTER[key]]
                else:
                    nrc, nlines = run_tables(nm, mdir, key)
                    if nrc == 0:
                        after = ['%s refuses this mod and %s (The Nightmare Levels) loads every table, so it is '
                                 'an add-on mod.  What %s refused:' % (GAMES[key], ADDON_GAMES[key], GAMES[key])]
                        after += ['  ' + ln for ln in lines if ln not in set(nlines)]
                        trc, lines, gname = 0, nlines, ADDON_GAMES[key]
                    else:
                        after = ['%s (The Nightmare Levels) refuses it too; for a mod of the add-on, check '
                                 '--addon prints the add-on\'s own lines' % ADDON_GAMES[key]]
            for line in lines:
                out('  ' + line)
            for line in after:
                out(line)
            if trc != 0:
                out('error: the game would refuse this mod, as above')
                rc = 1
        nwarn = sum('WARNING' in line for line in lines)

        known = {n.lower() for n in TABLES[key] + ALWAYS}
        # The scripts' modules (API 7): Scripts\a\b.lua is read when a script requires a.b, which the
        # checker's summary names ("...; modules a.b, c")
        required = set()
        for line in lines:
            m = re.search(r'Script\.lua: .*; modules ([\w., ]+)', line)
            if m:
                for name in m.group(1).split(','):
                    required.add(('scripts' + os.sep + name.strip().replace('.', os.sep) + '.lua').lower())
        for dp, _dn, fn in os.walk(mdir):
            for n in sorted(fn):
                rel = os.path.relpath(os.path.join(dp, n), mdir)
                if rel.lower() in required:
                    continue
                if os.sep in rel or n.lower() not in known:
                    out('warning: Mod\\%s is not a file %s reads; it is signed, but nothing opens it'
                        % (rel.replace(os.sep, '\\'), gname))
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
            out('ok: %s would load this mod%s' % (gname, ', with %d script warning%s above' % (
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

        # Which harness a Blood II mod is checked against: a stub stands in for the two table tests, the
        # base game knowing BERETTA and the add-on BERETTA and COMBATSHOTGUN, as the real ones do.
        global RUN
        build = os.path.join(t, 'build')
        os.makedirs(os.path.join(build, 'OUT'))
        for b in (TESTER['blood2'], ADDON_TESTER['blood2'], TESTER['shogo']):
            open(os.path.join(build, 'OUT', b), 'w').close()
        ran = []

        def stub(tester, run_root):
            name = os.path.basename(tester)
            ran.append(name)
            with open(os.path.join(run_root, 'Mod', 'Weapons.txt')) as f:
                weapon = f.read().strip()[1:-1]
            if weapon in (['BERETTA', 'COMBATSHOTGUN'] if name == ADDON_TESTER['blood2'] else ['BERETTA']):
                return 0, ['Mod\\Weapons.txt: %s: MinDamage 1.0' % weapon]
            return 1, ['Mod\\Weapons.txt: [%s] is not a Blood 2 weapon' % weapon]

        def b2mod(name, weapon, game='Blood2'):
            os.makedirs(os.path.join(t, name, 'Mod'))
            with open(os.path.join(t, name, 'Mod', 'Mod.txt'), 'w') as f:
                f.write('[Mod]\nId = "T"\nGame = "%s"\n' % game)
            with open(os.path.join(t, name, 'Mod', 'Weapons.txt'), 'w') as f:
                f.write('[%s]\n' % weapon)
            return os.path.join(t, name)

        def run(mod, **kw):
            del ran[:]
            lines = []
            return check(mod, build, out=lines.append, **kw), lines, list(ran)

        saved, RUN = RUN, stub
        try:
            rc, lines, r = run(b2mod('base', 'BERETTA'))
            expect(rc == 0 and r == ['testBlood2Tables'] and lines[-1] == 'ok: Blood2 would load this mod',
                   'a base-game mod is checked against the base game alone')
            rc, lines, r = run(b2mod('nm', 'COMBATSHOTGUN'))
            expect(rc == 0 and r == ['testBlood2Tables', 'testBlood2TablesNM'] and
                   lines[-1] == 'ok: Blood2NM would load this mod' and
                   '  Mod\\Weapons.txt: [COMBATSHOTGUN] is not a Blood 2 weapon' in lines,
                   'a mod the base game refuses and the add-on takes is an add-on mod, with what the base refused')
            rc, lines, r = run(b2mod('typo', 'BERRETTA'))
            expect(rc == 1 and len(r) == 2 and 'refuses it too' in ' '.join(lines) and
                   not any(ln.startswith('ok') for ln in lines), 'a mod both refuse is refused')
            rc, lines, r = run(os.path.join(t, 'base'), addon=True)
            expect(rc == 0 and r == ['testBlood2TablesNM'] and lines[-1] == 'ok: Blood2NM would load this mod',
                   '--addon checks against the add-on alone')
            rc, lines, r = run(b2mod('shogo', 'BERETTA', 'Shogo'), addon=True)
            expect(rc == 2 and r == [], 'and Shogo has none')
            os.remove(os.path.join(build, 'OUT', ADDON_TESTER['blood2']))
            rc, lines, r = run(os.path.join(t, 'nm'))
            expect(rc == 1 and r == ['testBlood2Tables'] and 'BUILD_BLOOD2_ADDON' in ' '.join(lines),
                   'without the add-on built, an add-on mod is refused, saying what to build')
        finally:
            RUN = saved
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
        addon = '--addon' in args
        rest = [a for a in args[1:] if a != '--addon']
        if len(rest) == 1:
            return check(rest[0], build, addon=addon)
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
