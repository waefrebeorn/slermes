#!/usr/bin/env python3
"""Rank Python modules by unported pure-function yield.

Earlier versions of this comparison matched a Python function name against C
FILE STEMS, so any module whose C port uses a prefix convention (cron/jobs.py ->
cronjobs_*, display.py -> st_*) scored as if NOTHING were ported. This version
matches against C IDENTIFIERS extracted from the whole src/ tree, which is what
"already ported" actually means.

Usage: python scripts/rank_pure_functions.py [--min N] [--top N]
"""
import argparse
import ast
import json
import re
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
QUARRY = Path("C:/Users/eman5/hermes-agent")

# Substrings that mark a function as not portable in a clean unit.
NOT_PURE = (
    "open(", "subprocess", "socket", "urllib", "requests", "logging",
    "os.environ", "sqlite3", "threading", "contextlib", "import ",
)

IDENT = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]{2,})\b")


def c_identifiers() -> set:
    """Every identifier mentioned anywhere in src/**/*.c (and include/*.h)."""
    out = set()
    for base in (REPO / "src", REPO / "include"):
        for p in base.rglob("*"):
            if p.suffix not in (".c", ".h"):
                continue
            try:
                out.update(IDENT.findall(p.read_text(encoding="utf-8",
                                                     errors="replace")))
            except OSError:
                pass
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--min", type=int, default=25)
    ap.add_argument("--top", type=int, default=15)
    args = ap.parse_args()

    ids = c_identifiers()
    scan = REPO / "live_parity_scan.json"
    modules = json.loads(scan.read_text(encoding="utf-8"))["modules"] \
        if scan.exists() else {}

    rows = []
    for name, rec in modules.items():
        rel = name if name.endswith(".py") else name + ".py"
        py = QUARRY / rel
        if not py.exists():
            py = REPO / rel
        if not py.exists():
            continue
        try:
            full = py.read_text(encoding="utf-8", errors="replace")
            tree = ast.parse(full)
        except (SyntaxError, OSError):
            continue

        free, lines = [], 0
        for node in tree.body:
            if not isinstance(node, ast.FunctionDef):
                continue
            if node.args.args and node.args.args[0].arg in ("self", "cls"):
                continue
            seg = ast.get_source_segment(full, node) or ""
            if any(m in seg for m in NOT_PURE):
                continue
            stem = node.name.strip("_")
            if stem in ids:          # <-- identifier match, not file stem
                continue
            free.append(node.name)
            lines += node.end_lineno - node.lineno + 1

        if len(free) >= args.min:
            rows.append((len(free), rec.get("real_gaps", 0), name, lines))

    rows.sort(key=lambda r: -r[0])
    print(f"unported pure fns (identifier-matched, min {args.min}):")
    for n, gaps, name, lines in rows[:args.top]:
        print(f"  {name:44} {n:4d} fns {lines:5d} lines  {gaps:5d} gaps")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
