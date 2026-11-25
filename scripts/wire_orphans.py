"""Wire orphans that are genuinely missing, and refuse the ones that are duplicates.

`orphan_audit.py` found 903 `.c` files absent from `build/objects.mk`. Compiling the first
tree (src/gateway, 88 files) showed 87 build clean -- but wiring all 87 into the link
produced **56 `multiple definition` errors**: those files are superseded splits whose
symbols already live in linked monoliths (`status.c`, `run.c`). So "compiles" is not
"missing".

This decides by symbol, not by hope: nm each candidate object, nm the linked objects, and
only a file with NO symbol already in the product is a real orphan. Duplicates are reported
as `superseded` and left alone, because the linked monolith is not obviously the wrong copy
and guessing which split is authoritative is a separate decision per symbol.

Usage:
  python scripts/wire_orphans.py                # classify, wire nothing
  python scripts/wire_orphans.py --write        # add the real orphans to objects.mk
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
NM = "nm"
DEFINED = re.compile(r"^[0-9a-fA-F]+\s+([TDBRW])\s+(\S+)$")


def linked_objects():
    return re.findall(r"(src/[\w./-]+\.o)", MK.read_text(encoding="utf-8", errors="replace"))


def symbols(obj: Path):
    """Global symbols this object DEFINES (T/D/B/R/W), ignoring local ones."""
    if not obj.exists():
        return set()
    proc = subprocess.run([NM, "-g", "--defined-only", str(obj)],
                          capture_output=True, text=True, errors="replace")
    out = set()
    for line in (proc.stdout or "").splitlines():
        m = DEFINED.match(line.strip())
        if m and m.group(2) not in ("__GLOBAL__sub_I", "main"):
            out.add(m.group(2))
    return out


def main(argv):
    write = "--write" in argv
    objs = set(linked_objects())
    have = set()
    for o in objs:
        have |= symbols(REPO / o)

    real, superseded, unbuilt = [], [], []
    for path in sorted((REPO / "src").rglob("*.c")):
        rel = path.relative_to(REPO).as_posix()
        if rel[:-2] + ".o" in objs:
            continue
        syms = symbols(REPO / (rel[:-2] + ".o"))
        if not syms:
            unbuilt.append(rel)
        elif syms & have:
            superseded.append((rel, len(syms & have)))
        else:
            real.append(rel)

    print("orphans checked : %d" % (len(real) + len(superseded) + len(unbuilt)))
    print("  real (no symbol overlap) : %d" % len(real))
    print("  superseded duplicates    : %d" % len(superseded))
    print("  not built (or empty)     : %d" % len(unbuilt))
    print("\nreal orphans, first 15:")
    for f in real[:15]:
        print("  ", f)
    print("\nsuperseded, first 10 (symbols already shipped):")
    for f, n in sorted(superseded, key=lambda t: -t[1])[:10]:
        print("  %-58s %d symbols" % (f, n))

    if write and real:
        text = MK.read_text(encoding="utf-8", errors="replace")
        add = " ".join(f[:-2] + ".o" for f in real)
        text = text.replace("src/infrastructure_stubs.o", add + " src/infrastructure_stubs.o", 1)
        MK.write_text(text, encoding="utf-8", newline="")
        print("\nwired %d objects into build/objects.mk" % len(real))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
