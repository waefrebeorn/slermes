#!/usr/bin/env python3
"""Differential oracle for hermes_agent/compat/paths.py (gate w22_paths_5ce33e).

The mapping engine is live: a `PathMoves` is built from the fixture table (the
AST read of a tree's moves.py is the source seam) and every method is called on
it:

  directory        -- live `_directory(path, table)`
  current          -- PathMoves.current
  original         -- PathMoves.original (the bounded undo walk)
  module           -- PathMoves.module
  original_module  -- PathMoves.original_module
  current_globs    -- list(PathMoves.current_globs(pattern))
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_agent" / "compat" / "paths.py"
GATE = "w22_paths_5ce33e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("compat_paths_live", SRC)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"directory", "current", "original", "module", "original_module",
          "current_globs"}


def expected(r):
    func = r.get("func")
    table = r.get("table") or {}
    moves = live.PathMoves(table)
    if func == "directory":
        return live._directory(r.get("path"), table)
    if func == "current":
        return moves.current(r.get("path"))
    if func == "original":
        return moves.original(r.get("path"))
    if func == "module":
        return moves.module(r.get("name"))
    if func == "original_module":
        return moves.original_module(r.get("name"))
    if func == "current_globs":
        return list(moves.current_globs(r.get("pattern")))
    raise AssertionError("unknown func %r" % (func,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
