"""Classify every .c file that carries no proven behaviour claim.

The audit reports 570 "pending headers" (a STATUS: PENDING / port_pending admission) and
2365 "no claim" files. Those two numbers are the honest denominator problem: until they are
sorted, "80.2% of the reference by bytes" is a file-size comparison, not a coverage figure.

Buckets, decided from the source text alone:

* todo      -- the file admits a pending behaviour port and holds real function bodies.
               This is unported work that knows it is unported.
* seam      -- the file touches the outside world (file, socket, process, TLS). Behaviour
               cannot be pinned by a pure value->value oracle; it needs an environment.
* outside   -- C-only trees with no Python counterpart to port from (cli, pet, lv, lua,
               desktop, compat, ports). Not a gap against hermes by definition.
* empty     -- no function bodies and no claim: a placeholder TU. Dead weight.
* done      -- has a real (non-placeholder) PoP line but no pending admission. Claimed,
               so it belongs in the ported count, not here.

Usage: python scripts/classify_surface.py [--json]
"""
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src"

STUB_RE = re.compile(r"(STATUS:\s*PENDING|port[ _]pending|PORT_PENDING)", re.IGNORECASE)
POP_RE = re.compile(r"^\s*\*?\s*PoP:", re.MULTILINE)
POP_PLACEHOLDER_RE = re.compile(r"^\s*\*?\s*PoP:.*_pending\b", re.MULTILINE)
# A closing brace at column 0 ends one top-level definition. Counting those is far more
# robust than pattern-matching return types: multi-line signatures and hand-wrapped
# prototypes made the first cut report 1346 "empty" files when only 92 really had no code.
DEF_RE = re.compile(r"^\}\s*$", re.MULTILINE)
IO_RE = re.compile(
    r"\b(fopen|freopen|socket|connect|SSL_|curl_easy|popen|system|CreateProcess|"
    r"sqlite3_open|read\s*\(|write\s*\(|recv\s*\(|send\s*\()"
)
# Trees with no counterpart in the Python package: nothing to be a port OF.
OUTSIDE = ("src/cli/", "src/pet/", "src/lv/", "src/lua/", "src/compat/", "src/ports/",
           "src/desktop", "src/app_desktop", "src/main", "src/provider/")


def classify():
    rows = []
    for path in sorted(SRC.rglob("*.c")):
        rel = path.relative_to(REPO).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        pops = len(POP_RE.findall(text))
        ph = len(POP_PLACEHOLDER_RE.findall(text))
        real = pops - ph
        defs = len(DEF_RE.findall(text))
        stub = bool(STUB_RE.search(text))
        io = bool(IO_RE.search(text))
        if real and not stub:
            bucket = "done"
        elif any(rel.startswith(p) for p in OUTSIDE):
            bucket = "outside"
        elif defs == 0:
            bucket = "empty"
        elif io:
            bucket = "seam"
        else:
            # Real bodies, no pending admission, no outside world: ported code that
            # carries no claim. First cut dropped these into "empty", which made 39% of
            # the tree look like dead placeholders when it was unclaimed work.
            bucket = "todo"
        rows.append({"file": rel, "bucket": bucket, "stub": stub, "io": io,
                     "pop": pops, "placeholder": ph, "real": real, "defs": defs,
                     "bytes": len(text)})
    return rows


def main(argv):
    rows = classify()
    counts, byes = {}, {}
    for r in rows:
        counts[r["bucket"]] = counts.get(r["bucket"], 0) + 1
        byes[r["bucket"]] = byes.get(r["bucket"], 0) + r["bytes"]
    order = ["done", "todo", "seam", "outside", "empty"]
    if "--json" in argv:
        print(json.dumps({"counts": counts, "bytes": byes}, indent=2))
        return 0
    total = len(rows)
    print("bucket    files    KB   share")
    for b in order:
        n = counts.get(b, 0)
        print("%-8s %6d %6.0f  %5.1f%%" % (b, n, byes.get(b, 0) / 1024,
                                           (n / total * 100) if total else 0))
    print("%-8s %6d %6.0f  100.0%%" % ("TOTAL", total, sum(byes.values()) / 1024))
    print()
    print("claimless = todo + seam + outside + empty = %d"
          % sum(counts.get(b, 0) for b in ("todo", "seam", "outside", "empty")))
    print("placeholder PoP lines across all files = %d"
          % sum(r["placeholder"] for r in rows))
    print("\ntodo (first 10, by size):")
    for r in sorted((r for r in rows if r["bucket"] == "todo"),
                    key=lambda r: -r["bytes"])[:10]:
        print("  %-64s %6.0f KB %d defs" % (r["file"], r["bytes"] / 1024, r["defs"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
