#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_process_dock.py pure helpers
(port include/port_cli_process_dock.c): _last_output_line, process_status,
process_glyph. Direct-import has ruamel risk (probe interpreter vs gate), so
ast-extract the three live fns + _STATUS_BY_REASON/_GLYPH constants and drive
live, reconstructing the dict each fn reads from the harness's parameterized
facts:
  lol: _last_output_line(buffer) -> last non-blank line
  st:  process_status(dict with status/exited, completion_reason, exit_code)
  gly: process_glyph(dict with status)
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/hermes_cli/cli_process_dock.py",
               encoding="utf-8").read()
    g = {"Any": None, "Dict": None}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in ("_last_output_line",
                                                         "process_status",
                                                         "process_glyph"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "cpd", "exec"), g)
        elif isinstance(n, ast.Assign):
            for tar in n.targets:
                if isinstance(tar, ast.Name) and tar.id in ("_STATUS_BY_REASON", "_GLYPH"):
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "cpd", "exec"), g)
    lol = g["_last_output_line"]
    pst = g["process_status"]
    pgl = g["process_glyph"]
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "lol":
            want = lol(c.get("buf"))
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH lol case{c.get('case')} C={c.get('out')!r} PY={want!r}")
        elif fam == "st":
            entry = {"status": "exited" if c.get("exited") else "running",
                     "completion_reason": c.get("reason") or "",
                     "exit_code": c.get("exit")}
            want = pst(entry)
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH st case{c.get('case')} C={c.get('out')!r} PY={want!r}")
        elif fam == "gly":
            want = pgl({"status": c.get("status")})
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH gly case{c.get('case')} C={c.get('out')!r} PY={want!r}")
    if total == 0:
        print("ORACLE cli_process_dock: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE cli_process_dock: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
