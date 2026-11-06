#!/usr/bin/env python3
"""Oracle for agent/repetition_guard.py pure repetition detectors.

Harness family (repetition_guard):
  dom {family, case, text, line:int, window:int, periodic:int,
       dominated:int, runaway:int}
Replays the live detectors on the same text:
  line      -> _line_repetition_dominated(text, n)
  window    -> _window_count_dominated(text, n)
  periodic  -> _periodic_run_dominated(text, n)
  dominated -> is_repetition_dominated(text)
  runaway   -> is_runaway_repetition(text)
n == len(text) the harness passes (ASCII fixtures). Seeds math + Counter +
the five constants, ast-extracts the funcs + _expand_run helper.
"""
import ast
import json
import math
import sys
from collections import Counter

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/repetition_guard.py"
CONSTS = ("MIN_FRAGMENT_LENGTH", "_REPEAT_WINDOW", "_MIN_REPEAT_COUNT",
          "_DOMINANCE_RATIO", "_RUNAWAY_DISTINCT_LINE_RATIO",
          "_MAX_ANCHOR_SAMPLES")
FUNS = ("_line_repetition_dominated", "_window_count_dominated",
        "_periodic_run_dominated", "_expand_run", "is_repetition_dominated",
        "is_runaway_repetition")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"math": math, "Counter": Counter}
    # grab ALL module-level Assign/AnnAssign constants (the detectors reference
    # several _* config values; seeding the whole constant surface avoids the
    # NameError chase). Skip names that are function-like or already covered.
    for n in ast.parse(src).body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            if any(isinstance(t, ast.Name) for t in targets):
                exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in FUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    line_dom = ns["_line_repetition_dominated"]
    window_dom = ns["_window_count_dominated"]
    periodic_dom = ns["_periodic_run_dominated"]
    dominated = ns["is_repetition_dominated"]
    runaway = ns["is_runaway_repetition"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE repetition_guard: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        txt = c.get("text")
        n = len(txt) if txt is not None else 0
        # live detectors treat non-str as not-dominated; a null harness case
        # must map to all False (C guards NULL -> 0 on every check)
        if txt is None:
            for name, have in (("line", c.get("line")), ("window", c.get("window")),
                               ("periodic", c.get("periodic")),
                               ("dominated", c.get("dominated")),
                               ("runaway", c.get("runaway"))):
                if bool(have) != False:
                    mism += 1
                    print(f"MISMATCH {name} case={c.get('case')} C={have} PY=False")
            continue
        pairs = [
            ("line", c.get("line"), line_dom(txt, n)),
            ("window", c.get("window"), window_dom(txt, n)),
            ("periodic", c.get("periodic"), periodic_dom(txt, n)),
            ("dominated", c.get("dominated"), dominated(txt)),
            ("runaway", c.get("runaway"), runaway(txt)),
        ]
        for name, have, want in pairs:
            if bool(have) != bool(want):
                mism += 1
                print(f"MISMATCH {name} case={c.get('case')} C={have} PY={want}")
    print(f"ORACLE repetition_guard: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
