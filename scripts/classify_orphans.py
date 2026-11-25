"""Classify the orphan translation units by symbol overlap with what is already linked.

Why this exists: linking all 903 orphans at once produced 604 duplicate definitions across 134
object pairs -- evidence that many orphans are *second generations* of files already in the
product (display_diff.o vs port_display_diff.o, jobs.o vs port_cron_jobs.o). Chasing that with
rebuild loops is a search problem solved by text editing. One nm pass answers it instead.

Buckets, per orphan object, comparing the symbols it DEFINES against the symbols the linked
(product) objects already define:

  DISJOINT  defines nothing the product already defines   -> genuinely new, safe to link
  SUBSET    every symbol is already defined elsewhere     -> duplicate generation, drop or archive
  SUPERSET  adds symbols on top of shared ones            -> replacement candidate for its twin
  PARTIAL   shares some symbols, adds others              -> per-symbol decision (merge)

Usage:
  python scripts/classify_orphans.py [--json]
"""
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"


def defined(obj: str):
    try:
        out = subprocess.run(["nm", "-g", "--defined-only", obj], cwd=str(REPO),
                             capture_output=True, text=True, errors="replace").stdout
    except Exception:
        return None
    syms = set()
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 3 and p[1] in "TDBRVWGS":
            syms.add(p[2])
    return syms


def main(argv):
    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = sorted(set(re.findall(r"(src/[\w./-]+\.o)", text)))

    owner = {}
    for o in linked:
        for s in defined(o) or ():
            owner.setdefault(s, o)

    orphan_list = sorted({l.strip()[:-2] + ".o"
                          for l in (REPO / ".testbin" / "orphan_all.txt").read_text().splitlines()
                          if l.strip()})
    orphan_list = [o for o in orphan_list if o not in set(linked)]

    buckets = {"DISJOINT": [], "SUBSET": [], "SUPERSET": [], "PARTIAL": [], "NO_OBJECT": []}
    detail = {}
    for o in orphan_list:
        if not (REPO / o).exists():
            buckets["NO_OBJECT"].append(o)
            continue
        syms = defined(o)
        if syms is None:
            buckets["NO_OBJECT"].append(o)
            continue
        shared = {s for s in syms if s in owner}
        if not syms:
            buckets["SUBSET"].append(o)
        elif not shared:
            buckets["DISJOINT"].append(o)
        elif shared == syms:
            buckets["SUBSET"].append(o)
        elif {s for s in owner if owner[s] == o} and shared and (syms - shared):
            buckets["PARTIAL"].append(o)
        else:
            buckets["SUPERSET"].append(o)
            # unreachable branch kept for clarity
        detail[o] = {"defined": len(syms), "shared": len(shared),
                     "new": len(syms - shared),
                     "twins": sorted({owner[s] for s in shared})[:3]}

    print("linked objects      : %d" % len(linked))
    print("orphan objects      : %d" % len(orphan_list))
    for k in ("DISJOINT", "SUBSET", "PARTIAL", "SUPERSET", "NO_OBJECT"):
        print("  %-10s %4d" % (k, len(buckets[k])))

    out = {"linked": len(linked), "orphans": len(orphan_list),
           "buckets": {k: v for k, v in buckets.items()}, "detail": detail}
    (REPO / ".testbin" / "orphan_classes.json").write_text(json.dumps(out, indent=1))
    if "--json" in argv:
        print(json.dumps({k: len(v) for k, v in buckets.items()}))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
