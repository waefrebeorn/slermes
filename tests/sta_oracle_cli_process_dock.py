#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_process_dock.py pure helpers: _last_output_line,
process_status, process_glyph. The three are ast-extracted live; the module
globals they read are pulled into the namespace too.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_process_dock.py"


def _ns():
    ns = {}
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    for n in tree.body:
        if isinstance(n, ast.Assign):
            for t in n.targets:
                if isinstance(t, ast.Name) and t.id in (
                        "_STATUS_BY_REASON", "_GLYPH"):
                    exec(compile(ast.Module(body=[n], type_ignores=[]),
                                 "x", "exec"), ns)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in (
                "_last_output_line", "process_status", "process_glyph"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return ns


NS = _ns()
_last_output_line = NS["_last_output_line"]
process_status = NS["process_status"]
process_glyph = NS["process_glyph"]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
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
        print("ORACLE cli_process_dock: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "lol":
            want = _last_output_line(c["buf"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH lol {c['case']}: C={c['out']!r} "
                      f"PY={want!r} buf={c['buf']!r}")
        elif fam == "st":
            entry = {"status": "exited" if c["exited"] else "running",
                     "completion_reason": c["reason"],
                     "exit_code": 1 if c["exit"] else 0}
            want = process_status(entry)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH st {c['case']}: C={c['out']!r} PY={want!r} "
                      f"exit={c['exit']} reason={c['reason']!r}")
        elif fam == "gly":
            row = {"status": c["status"]}
            want = process_glyph(row)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH gly {c['case']}: C={c['out']!r} PY={want!r}")
    print(f"ORACLE cli_process_dock: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
