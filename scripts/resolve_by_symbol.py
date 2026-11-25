"""Decide, by symbol, which objects must stay linked when two objects collide.

The blunt rule "drop the orphan copy of a duplicated pair" removed 104 objects and left 34
undefined references: several of those objects were the only provider of symbols their
counterpart never defined. Dropping the pair is not enough -- the drop is only safe when every
symbol the dropped object defines is also defined by something still linked.

This computes that: for each dropped object, look at its defined globals and ask whether another
linked object defines each one. If any symbol is uncovered, the object must go back in.

Usage:
  python scripts/resolve_by_symbol.py            # report
  python scripts/resolve_by_symbol.py --write    # re-add the necessary ones to objects.mk
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"


def defined_symbols(obj: str) -> set:
    try:
        out = subprocess.run(["nm", "-g", "--defined-only", obj], cwd=str(REPO),
                             capture_output=True, text=True, errors="replace").stdout
    except Exception:
        return set()
    syms = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] in "TDBRVWGS":
            syms.add(parts[2])
    return syms


def main(argv):
    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = set(re.findall(r"(src/[\w./-]+\.o)", text))
    dropped_file = REPO / ".testbin" / "dropped_dupes.txt"
    dropped = [l.strip() for l in dropped_file.read_text().splitlines() if l.strip()] \
        if dropped_file.exists() else []

    owner = {}
    for o in sorted(linked):
        for s in defined_symbols(o):
            owner.setdefault(s, []).append(o)

    readd, keep_out = [], []
    for o in dropped:
        syms = defined_symbols(o)
        uncovered = [s for s in syms if s not in owner]
        (readd if uncovered else keep_out).append((o, uncovered))

    print("linked objects      : %d" % len(linked))
    print("dropped candidates  : %d" % len(dropped))
    print("must be re-added    : %d" % len(readd))
    print("safe to stay out    : %d" % len(keep_out))
    for o, u in sorted(readd, key=lambda x: -len(x[1])):
        print("  + %-52s provides %d unique: %s" % (o, len(u), ", ".join(sorted(u)[:4])))

    if "--write" in argv and readd:
        add = [o for o, _ in readd]
        marker = "CUPBOARD_ORPHANS = "
        i = text.find(marker)
        if i < 0:
            print("CUPBOARD_ORPHANS not found", file=sys.stderr)
            return 1
        j = i + len(marker)
        text = text[:j] + " ".join(add) + " " + text[j:]
        MK.write_text(text, encoding="utf-8", newline="")
        print("re-added %d object(s) to objects.mk" % len(add))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
