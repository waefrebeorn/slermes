#!/usr/bin/env python3
"""
pure_scan.py — find the next oracle-drivable pure-function port.

Scans src/cli/cli_gaps.c PoP stubs against the live hermes-agent Python and
scores each candidate for how portable it is *right now*, so the agent can
start wiring the next oracle without first re-running the same qualified
greps.

Scoring (higher = better pure port candidate):
  +3 deterministic return (has a `return <expr>`, no writes/IO in body)
  +2 short body (<= 10 statements)  — small surface to mirror
  +2 scalar args only (params not self/cls and plain names) 
  +1 pure-control-flow only (if/for/return/math, no calls at all)
  -1 any t( i18n / runtime. / env / Path / open( / await / logger (likely
     runtime-coupled)  [skips at -1]
  -1 any self.<attr> write / global mutation / class body (stateful)

Defaults to excluding obvious runtime-coupled candidates; --include-runtime
shows them anyway ranked low. Output is one ranked line per candidate.

Usage:
  python tests/pure_scan.py [--limit N] [--include-runtime] [--by-module]
"""

import argparse
import ast
import re
import sys
from pathlib import Path

SLERMES = Path("C:/Users/eman5/slermes")
HERMES = Path("C:/Users/eman5/hermes-agent")

GAPS = SLERMES / "src" / "cli" / "cli_gaps.c"

STUB_RE = re.compile(r"/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+\.py):([\w.]+)\s*\*/")

RUNTIME = ("t(", "runtime.", "os.", "env", "Path(", "open(", "await ", "logger.",
           "subprocess", "asyncio", "socket", ".write(", ".read(", "getenv",
           "get_hermes_home", "current(", "listdir", "glob(", "time.", "random.",
           "input(", "print(")
DECISION = ("if", "for", "return", "min(", "max(", "abs(", "sum(", "==", "!=",
            ">", "<", "strip(", "split(", "lower(", "upper(", "replace(",
            "join(", "startswith", "endswith", "in ", "not ", "and ", "or ")
STATE = ("self.", "global ", "yield", "async def")


def load_live(mod: str, func: str):
    srcp = HERMES / mod
    if not srcp.is_file():
        return None
    try:
        tree = ast.parse(srcp.read_text(encoding="utf-8", errors="replace"))
    except SyntaxError:
        return None
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == func:
            return ast.get_source_segment(srcp.read_text(encoding="utf-8",
                                                         errors="replace"), node)
    return None


def score(body: str) -> int:
    if any(k in body for k in RUNTIME):
        return -1
    for k in STATE:
        if k in body:
            return -1
    s = 0
    nstmt = body.count("\n")
    if re.search(r"return .*[+\-*/%]|return [a-zA-Z_]", body):
        s += 3
    if nstmt <= 10:
        s += 2
    # A call to a non-builtin identifier is a runtime boundary: the callee may do
    # IO/state and is itself another thing to port. Demote hard.
    PURE_BUILTINS = {"str", "int", "float", "bool", "len", "any", "all", "min",
                     "max", "abs", "sum", "round", "repr", "hash", "lower",
                     "upper", "strip", "rstrip", "lstrip", "split", "rsplit",
                     "replace", "join", "startswith", "endswith", "format",
                     "reversed", "sorted", "enumerate", "zip", "range"}
    called = set(re.findall(r"(\b[a-z_]\w*)\s*\(", body))
    nonbuilt = called - PURE_BUILTINS - {"def", "if", "for", "while"}
    for tok in DECISION:
        if tok in body:
            s += 1
            break
    if re.search(r"return [\w.\"'()]+$", body):  # plain return expr
        s += 1
    if nonbuilt:
        s = max(s, 0) - 2  # real callee = runtime boundary; keep pure above it relative
    return s


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=40)
    ap.add_argument("--include-runtime", action="store_true")
    ap.add_argument("--by-module", action="store_true")
    ap.add_argument("--only", default=None, help="substring filter on module path")
    args = ap.parse_args()

    if not GAPS.is_file():
        print(f"cli_gaps.c not found: {GAPS}", file=sys.stderr)
        return 1
    gaps = GAPS.read_text(encoding="utf-8", errors="replace")
    cands = []
    for cname, mod, func in STUB_RE.findall(gaps):
        if args.only and args.only not in mod:
            continue
        body = load_live(mod, func)
        if not body:
            continue
        s = score(body)
        if s < 0 and not args.include_runtime:
            continue
        if s <= 0 and not args.include_runtime:
            continue
        cands.append((s, mod, func, body.count("\n")))
    cands.sort(key=lambda x: -x[0])
    if args.by_module:
        by = {}
        for s, mod, func, n in cands:
            by.setdefault(mod, []).append((s, func, n))
        for mod in sorted(by, key=lambda m: -max(x[0] for x in by[m])):
            print(f"{mod}  ({len(by[mod])} candidates)")
            for s, func, n in sorted(by[mod], key=lambda x: -x[0])[:6]:
                print(f"    {s:+d}  {func:<34s} {n} lines")
    else:
        for s, mod, func, n in cands[: args.limit]:
            print(f"{s:+d}  {func:<34s} {n:3d} ln  {mod}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
