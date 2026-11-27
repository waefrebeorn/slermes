#!/usr/bin/env python3
"""audit_stub_functions.py — find C functions that ignore their arguments.

The codex gate exposed the pattern that makes a gate meaningless: the port function
took an argument, never read it, and returned a constant. The gate then compares a
constant against a live answer -- either it mismatches forever, or (worse) it agrees
by accident and reads green.

This walks every built source, and for each top-level definition checks whether any
parameter identifier appears in the body. Definitions whose body never mentions a
parameter are candidates: either a stub, or a function that legitimately ignores a
parameter (a signature kept for compatibility). The join with gate provenance tells
you which of them are actually wrapped by a gate.

Usage:
  python scripts/audit_stub_functions.py [--json out.json] [--gated] [--limit N]
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
ROWS = REPO / "tests" / "watercup" / "build" / "gate_provenance.json"

SIG = re.compile(r"^(?:[A-Za-z_][\w \t\*]*?)\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{", re.M)
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else", "case"}


def functions_in(path: Path):
    try:
        src = path.read_text(errors="ignore")
    except OSError:
        return
    # strip comments and string literals so a param name in a comment does not count
    src = re.sub(r"/\*.*?\*/", lambda m: " " * len(m.group(0)), src, flags=re.S)
    src = re.sub(r"//[^\n]*", lambda m: " " * len(m.group(0)), src)
    for m in SIG.finditer(src):
        name, params = m.group(1), m.group(2)
        if name in KEYWORDS:
            continue
        start = m.end() - 1
        depth, i = 0, start
        while i < len(src):
            if src[i] == "{":
                depth += 1
            elif src[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = src[start:i]
        ids = re.findall(r"\b[A-Za-z_]\w*\b", params)
        names = [p for p in ids if p not in ("void", "const", "char", "int", "long",
                                             "unsigned", "signed", "short", "float",
                                             "double", "size_t", "bool", "struct",
                                             "FILE", "*", "restrict", "...")]
        # drop array brackets / pointer stars that leaked in
        names = [re.sub(r"\W", "", p) for p in names]
        names = [p for p in dict.fromkeys(names) if p and not p.isdigit()]
        if not names:
            continue
        used = set(re.findall(r"\b[A-Za-z_]\w*\b", body))
        if not any(p in used for p in names):
            yield {"symbol": name, "file": str(path.relative_to(REPO)),
                   "line": src[:m.start()].count("\n") + 1,
                   "params": names}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default="")
    ap.add_argument("--gated", action="store_true", help="only symbols a gate calls")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    gated = set()
    if args.gated or ROWS.exists():
        try:
            for r in json.loads(ROWS.read_text()):
                if r.get("class") in ("PORT", "MISSING", "COPY"):
                    gated.add(r["symbol"])
        except (OSError, ValueError):
            pass

    rows = []
    for root in ("src", "lib"):
        for c in sorted((REPO / root).rglob("*.c")):
            rows.extend(functions_in(c))
    if args.gated:
        rows = [r for r in rows if r["symbol"] in gated]
    rows.sort(key=lambda r: (r["symbol"] not in gated, r["file"]))
    if args.limit:
        rows = rows[: args.limit]

    for r in rows:
        flag = "GATED " if r["symbol"] in gated else "      "
        print("%s%-58s %s:%d  (params: %s)"
              % (flag, r["symbol"][:58], r["file"], r["line"], ", ".join(r["params"])))
    print("\n%d function(s) ignore every parameter; %d of them are called by a gate"
          % (len(rows), sum(1 for r in rows if r["symbol"] in gated)))
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))
        print("wrote", args.json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
