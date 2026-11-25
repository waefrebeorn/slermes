"""Resolve duplicate symbol definitions by namespacing the losing copy -- never by dropping a file.

The requirement for this tree is absolute: every ported translation unit must be linked into the
product. That makes `multiple definition` errors a *symbol identity* problem, not a dependency
problem. Two files define `display_diff_rows`; the linker is right to refuse, and the answer is
not to unlink one of them. The answer is that the second generation gets its own name.

Rule applied per duplicated symbol:
  * keeper  -- the copy whose name stays. Baseline (already-shipped) copy wins; if both or
               neither is baseline, the copy that other objects actually reference wins.
  * losers  -- every other definer. Inside that file only, `sym` is renamed
               `sym__<path-tag>`, definition and internal uses alike.
Callers elsewhere keep using `sym` and therefore bind to the keeper -- no caller edits, no
behaviour invented, and the losing implementation is still present in the binary.

Usage:
  python scripts/namespace_duplicates.py            # report only
  python scripts/namespace_duplicates.py --write    # rewrite the loser sources
"""
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
MANIFEST = REPO / ".testbin" / "renames.json"


_CACHE = {}


def scan(obj):
    """One nm invocation per object: returns (defined_globals, referenced_symbols)."""
    if obj in _CACHE:
        return _CACHE[obj]
    try:
        out = subprocess.run(["nm", "-g", obj], cwd=str(REPO),
                             capture_output=True, text=True, errors="replace").stdout
    except Exception:
        out = ""
    d, u = set(), set()
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3 and p[1] in "TDBRVWGS":
            # PE/COFF emits pseudo-symbols (.refptr.X, .rdata$.refptr.X) that are not C
            # identifiers; they can never be renamed in source and never collide as code.
            if p[2].startswith(".") or p[2].startswith("$"):
                continue
            d.add(p[2])
        elif len(p) == 2 and p[0] == "U":
            u.add(p[1])
        elif len(p) == 3 and p[0] == "U":
            u.add(p[2])
    _CACHE[obj] = (d, u)
    return _CACHE[obj]


def defined(obj):
    return scan(obj)[0]


def referenced(obj):
    return scan(obj)[1]


def tag_for(obj):
    return re.sub(r"[^A-Za-z0-9]+", "_", obj[:-2]).strip("_")


def main(argv):
    write = "--write" in argv
    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = sorted(set(re.findall(r"(src/[\w./-]+\.o)", text)))
    m = re.search(r"CUPBOARD_ORPHANS\s*=\s*((?:[^\n]*\\\n)*[^\n]*)", text)
    orphans = set(re.findall(r"(src/[\w./-]+\.o)", m.group(1))) if m else set()
    baseline = {o for o in linked if o not in orphans}

    definers = {}
    users = {}
    for o in linked:
        d, u = scan(o)
        users[o] = u
        for s in d:
            definers.setdefault(s, []).append(o)

    dups = {s: objs for s, objs in definers.items() if len(objs) > 1}

    ENTRY = {"main", "wmain", "SDL_main", "WinMain", "wWinMain", "DllMain"}
    plan = {}
    for sym, objs in sorted(dups.items()):
        if sym in ENTRY:
            continue
        base = [o for o in objs if o in baseline]
        if base:
            keeper = base[0]
        else:
            used = [o for o in objs if any(sym in users[u] for u in linked if u != o)]
            keeper = used[0] if used else objs[0]
        losers = [o for o in objs if o != keeper]
        plan[sym] = {"keep": keeper, "rename": losers}

    n_loose = sum(len(v["rename"]) for v in plan.values())
    print("linked objects     : %d (baseline %d, orphan %d)"
          % (len(linked), len(baseline), len(linked) - len(baseline)))
    print("duplicated symbols : %d" % len(plan))
    print("definitions to rename: %d" % n_loose)

    if not write:
        for sym in list(plan)[:15]:
            print("  %-44s keep %-40s rename %d" % (sym[:44], plan[sym]["keep"][:40], len(plan[sym]["rename"])))
        return 0

    manifest = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {}
    touched = {}
    for sym, info in plan.items():
        for loser in info["rename"]:
            src = REPO / (loser[:-2] + ".c")
            if not src.exists():
                continue
            new = "%s__%s" % (sym, tag_for(loser))
            changed = touched.setdefault(str(src), {})
            changed[sym] = new
            manifest.setdefault(sym, {})[loser] = new

    total_edits = 0
    for path, mapping in touched.items():
        p = Path(path)
        s = p.read_text(encoding="utf-8", errors="replace")
        before = s
        for sym, new in mapping.items():
            s = re.sub(r"(?<![\w])" + re.escape(sym) + r"(?![\w])", new, s)
        if s != before:
            p.write_text(s, encoding="utf-8", newline="")
            total_edits += 1
    MANIFEST.write_text(json.dumps(manifest, indent=1))
    print("files rewritten    : %d" % len(touched))
    print("files actually rewritten: %d (of %d considered)" % (total_edits, len(touched)))
    print("manifest           : %s" % MANIFEST.relative_to(REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
