"""Resolve duplicate definitions by making the losing copy local to its own object.

The requirement for this tree: every ported translation unit is linked, no exclusions. So a
`multiple definition` error cannot be answered by unlinking a file. It also should not be
answered by rewriting sources -- renaming symbols in .c files breaks call sites that legitimately
refer to the surviving copy (and, if the identifier happens to be part of an include path, it
breaks the build in ways that have nothing to do with linking).

`objcopy --localize-symbol` changes the loser's symbol binding from global to local in the object
file. A local symbol cannot collide with a global of the same name, the object stays in the link,
its own code still calls its own copy, and every other translation unit's reference binds to the
keeper. Nothing is dropped and no source is touched.

Keeper rule: the copy already in the product (outside CUPBOARD_ORPHANS) wins; otherwise the copy
that other objects actually reference. Process entry points are never localized.

Usage:
  python scripts/localize_dupes.py            # report
  python scripts/localize_dupes.py --write    # apply objcopy
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
OBJCOPY = "objcopy"
ENTRY = {"main", "wmain", "SDL_main", "WinMain", "wWinMain", "DllMain"}

_CACHE = {}


def scan(obj):
    if obj in _CACHE:
        return _CACHE[obj]
    try:
        out = subprocess.run(["nm", "-g", obj], cwd=str(REPO),
                             capture_output=True, text=True, errors="replace").stdout
    except Exception:
        out = ""
    d, u = {}, set()
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "TDBRVWGS":
            if p[2].startswith((".", "$")):
                continue
            d[p[2]] = p[1]
        elif len(p) == 2 and p[0] == "U":
            u.add(p[1])
    _CACHE[obj] = (d, u)
    return _CACHE[obj]


def main(argv):
    write = "--write" in argv
    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = sorted(set(re.findall(r"((?:src|lib)/[\w./-]+\.o)", text)))
    # Library objects are not spelled out in objects.mk -- they arrive through $(LIB_OBJ_FILTERED),
    # defined in build/libs-config.mk. Without them the scan sees only one definer per duplicate
    # pair and localizes nothing, which is exactly the 6 remaining conflicts.
    libs = REPO / "build" / "libs-config.mk"
    if libs.exists():
        linked = sorted(set(linked) | set(re.findall(r"(lib/[\w./-]+\.o)",
                                                  libs.read_text(encoding="utf-8", errors="replace"))))
    m = re.search(r"CUPBOARD_ORPHANS\s*=\s*((?:[^\n]*\\\n)*[^\n]*)", text)
    orphans = set(re.findall(r"((?:src|lib)/[\w./-]+\.o)", m.group(1))) if m else set()
    baseline = {o for o in linked if o not in orphans}

    definers, users = {}, {}
    for o in linked:
        if not (REPO / o).exists():
            continue
        d, u = scan(o)
        users[o] = u
        for s in d:
            definers.setdefault(s, []).append(o)

    dups = {s: objs for s, objs in definers.items() if len(objs) > 1}
    per_object = {}
    keep_report = []
    for sym, objs in sorted(dups.items()):
        if sym in ENTRY:
            # The process entry point must be the CLI's own main()/WinMain(), never a variant
            # (src/main_desktop.c is the desktop launcher and must not become the entry).
            pref = [o for o in objs if o == "src/main.o"] or \
                   [o for o in objs if o in baseline] or sorted(objs)
            keeper = pref[0]
        elif [o for o in objs if o.startswith("lib/")]:
            keeper = [o for o in objs if o.startswith("lib/")][0]
        elif [o for o in objs if o in baseline]:
            keeper = [o for o in objs if o in baseline][0]
        else:
            used = [o for o in objs if any(sym in users.get(u, ()) for u in linked if u != o)]
            keeper = used[0] if used else objs[0]
        for loser in objs:
            if loser != keeper:
                per_object.setdefault(loser, []).append(sym)
        keep_report.append((sym, keeper, [o for o in objs if o != keeper]))

    print("linked objects       : %d (baseline %d, orphan %d)"
          % (len(linked), len(baseline), len(linked) - len(baseline)))
    print("duplicated symbols   : %d" % len(dups))
    print("objects to localize  : %d" % len(per_object))
    print("symbols to localize  : %d" % sum(len(v) for v in per_object.values()))

    if not write:
        for sym, k, l in keep_report[:12]:
            print("  %-40s keep %-38s localize in %d" % (sym[:40], k[:38], len(l)))
        return 0

    changed = failed = 0
    for obj, syms in sorted(per_object.items()):
        cmd = [OBJCOPY]
        for s in syms:
            cmd += ["--localize-symbol=" + s]
        cmd.append(obj)
        r = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True, errors="replace")
        if r.returncode == 0:
            changed += 1
        else:
            failed += 1
            print("  FAILED %s: %s" % (obj, (r.stderr or "").strip()[:120]))
    print("objects localized    : %d" % changed)
    print("objcopy failures     : %d" % failed)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
