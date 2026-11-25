"""Restore global binding for symbols that lost it during deduplication.

Background: duplicates are resolved with `objcopy --localize-symbol`, which makes the losing
copy local to its object. That is safe for the symbol being deduplicated, but a symbol can be
localized in the copy that other translation units were actually linking against -- and then it
becomes unreachable and the link reports `undefined reference`.

This pass takes the linker's own complaint list and re-globalizes exactly those symbols, in the
object that should own them (preferring an object outside CUPBOARD_ORPHANS).

Usage:
  python scripts/reglobalize.py --log .testbin/p3b.log [--write]
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
SKIP_PREFIX = ("__mingw", "_MINGW", "_gnu_", "__imp_")


def all_defs(obj):
    try:
        out = subprocess.run(["nm", obj], cwd=str(REPO), capture_output=True,
                             text=True, errors="replace").stdout
    except Exception:
        return {}
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "TDBRVWGStdrwgs":
            d[p[2]] = p[1]
    return d


def main(argv):
    write = "--write" in argv
    log = Path(argv[argv.index("--log") + 1]) if "--log" in argv else REPO / ".testbin" / "p3b.log"
    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = sorted(set(re.findall(r"(src/[\w./-]+\.o)", text)))
    m = re.search(r"CUPBOARD_ORPHANS\s*=\s*((?:[^\n]*\\\n)*[^\n]*)", text)
    orphans = set(re.findall(r"(src/[\w./-]+\.o)", m.group(1))) if m else set()

    logtext = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
    allow_all = "--all" in argv
    syms = sorted({s for s in re.findall(r"undefined reference to `([^']+)'", logtext)
                   if allow_all or not s.startswith(SKIP_PREFIX)})
    print("undefined symbols needing a global definition: %d" % len(syms))

    defs = {}
    for o in linked:
        if (REPO / o).exists():
            for s, kind in all_defs(o).items():
                defs.setdefault(s, []).append((o, kind))

    todo = []
    for s in syms:
        cands = defs.get(s, [])
        winners = sorted(cands, key=lambda c: (c[0] in orphans, c[1].islower()))
        if not winners:
            print("  %-32s NO DEFINER in the link" % s)
            continue
        owner = winners[0][0]
        todo.append((s, owner))
        print("  %-32s -> %s" % (s, owner))

    if write:
        by_obj = {}
        for s, o in todo:
            by_obj.setdefault(o, []).append(s)
        ok = bad = 0
        for o, ss in by_obj.items():
            cmd = ["objcopy"] + ["--globalize-symbol=" + s for s in ss] + [o]
            r = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True, errors="replace")
            if r.returncode == 0:
                ok += 1
            else:
                bad += 1
                print("  FAILED %s: %s" % (o, (r.stderr or "").strip()[:100]))
        print("objects re-globalized: %d (failures %d)" % (ok, bad))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
