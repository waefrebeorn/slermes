#!/usr/bin/env python3
"""audit_gate_provenance.py — where does a gate's C answer actually come from?

A cup gate prints two numbers: one from a C program, one from live Python. The
Python side is the oracle. The C side is only EVIDENCE ABOUT THE PORT when it came
out of the port. Three shapes exist in this tree, and they look identical in a
green report:

  PORT   harness.c references a symbol defined in src/ (or lib/) and does not
         define it itself          -> the cups measure the shipped port
  COPY   harness.c defines the traced function locally ("traced copy of
         src/...")                 -> the cups measure a hand transcription made
                                      by the same agent that wrote the test, so a
                                      green line proves the TRANSCRIPTION agrees
                                      with Python, not the port
  SHIM   a gate-local object supplies the symbol the port also defines
         (see audit_gate_shims.py) -> the cups measure the gate

Only PORT gates are parity evidence. The COPY class is large; the point of this
script is to make it countable and to keep it from growing silently.

Usage:
  python scripts/audit_gate_provenance.py                # summary
  python scripts/audit_gate_provenance.py --list COPY    # names, one per line
  python scripts/audit_gate_provenance.py --json out.json
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ARCHIVE = REPO / "tests" / "watercup" / "build" / "_all_objects.a"

DEF_RE = re.compile(r"^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
C_KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "do", "else", "main",
    "printf", "fprintf", "snprintf", "sprintf", "memset", "memcpy", "strlen",
    "strcmp", "strncmp", "strcpy", "strncpy", "strtok_r", "atoi", "atof",
    "strtod", "malloc", "calloc", "realloc", "free", "tmpfile",
}


def port_symbols() -> set[str]:
    if not ARCHIVE.exists():
        print("missing %s -- build the port first" % ARCHIVE, file=sys.stderr)
        return set()
    r = subprocess.run(["nm", "-g", "--defined-only", str(ARCHIVE)],
                       capture_output=True, text=True, errors="replace")
    out = set()
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            out.add(parts[2])
    return out


def classify(gdir: Path, port: set[str], shim_names: set[str]) -> tuple[str, str]:
    g = json.loads((gdir / "gate.json").read_text())
    cfn = g.get("c_fn") or ""
    try:
        src = (gdir / "harness.c").read_text(errors="ignore")
    except OSError:
        return "BROKEN", "no harness.c"
    txt = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    defined = {m.group(1) for m in DEF_RE.finditer(txt)} - C_KEYWORDS
    base = cfn.split(".")[-1]
    if base and base in defined:
        return "COPY", base
    if base and base in port:
        return "PORT", base
    if base and any(s in shim_names for s in (base,)):
        return "SHIM", base
    if cfn and re.search(r"\b%s\s*\(" % re.escape(base), txt):
        return "PORT?", base  # referenced, not defined here, not in the archive
    return "UNKNOWN", base


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", metavar="CLASS", help="print gate names of one class")
    ap.add_argument("--json", metavar="PATH", help="write per-gate rows")
    args = ap.parse_args()

    port = port_symbols()
    rows = []
    for gdir in sorted(GATES.iterdir()):
        if not (gdir / "gate.json").is_file():
            continue
        shim_names = set()
        shim = gdir / "shims.c"
        if shim.exists():
            txt = shim.read_text(errors="ignore")
            shim_names = {m.group(1) for m in DEF_RE.finditer(txt)} - C_KEYWORDS
        cls, sym = classify(gdir, port, shim_names)
        rows.append({"gate": gdir.name, "class": cls, "symbol": sym})

    counts = Counter(r["class"] for r in rows)
    if args.list:
        for r in rows:
            if r["class"] == args.list:
                print(r["gate"])
        return 0
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))
        print("wrote", args.json)
    total = len(rows)
    for cls, n in counts.most_common():
        print("%-8s %6d  %5.1f%%" % (cls, n, 100.0 * n / max(total, 1)))
    print("total    %6d" % total)
    print("\nPORT is parity evidence; COPY measures a hand transcription; "
          "SHIM measures the gate. Only COPY->PORT conversion changes what a green means.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
