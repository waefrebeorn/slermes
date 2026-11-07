#!/usr/bin/env python3
"""Oracle for agent/display.py shell-summarization helpers
(port src/cli/port_agent_display.c). Direct-interpreter import diverges (probe
interpreter has deps the gate lacks / wrong-name earlier), so ast-extract the
live fns + transitive helpers + _SHELL_PIPE_TAIL_HEADS and drive live:
  split_words: _split_shell_words
  strip_pipe:  _strip_shell_pipe_tail
  clean_seg:   _clean_shell_segment
  compound:    _split_shell_compound
  bool:        _is_shell_boundary_echo (1/0 bool family)
"""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/display.py"
NAMES = {"_scan_quoted", "_split_shell_words", "_strip_shell_pipe_tail",
         "_split_shell_compound", "_clean_shell_segment", "_is_shell_boundary_echo",
         "_shell_basename", "_SHELL_PIPE_TAIL_HEADS"}


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    g = {"re": re, "Iterator": __import__("collections.abc", fromlist=["Iterator"]).Iterator,
         "Tuple": None, "Match": None, "Any": None, "Optional": None}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in NAMES:
            exec(compile(ast.Module(body=[n], type_ignores=[]), "disp", "exec"), g)
        elif isinstance(n, ast.Assign):
            for tar in n.targets:
                if isinstance(tar, ast.Name) and tar.id == "_SHELL_PIPE_TAIL_HEADS":
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "disp", "exec"), g)
    missing = NAMES - set(g)
    if missing:
        print("ORACLE display_shell: missing", sorted(missing))
        return 1
    f_sw = g["_split_shell_words"]
    f_sp = g["_strip_shell_pipe_tail"]
    f_cs = g["_clean_shell_segment"]
    f_cp = g["_split_shell_compound"]
    f_be = g["_is_shell_boundary_echo"]
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
        fn = c.get("fn")
        if fn == "split_words":
            want = f_sw(c.get("in") or "")
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH split_words C={c.get('out')} PY={want}")
        elif fn == "strip_pipe":
            want = f_sp(c.get("in") or "")
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH strip_pipe in={c.get('in')!r} C={c.get('out')!r} PY={want!r}")
        elif fn == "clean_seg":
            want = f_cs(c.get("in") or "")
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH clean_seg C={c.get('out')!r} PY={want!r}")
        elif fn == "compound":
            want = f_cp(c.get("in") or "")
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH compound C={c.get('out')} PY={want}")
        elif fn == "boundary" or fn == "is_boundary":
            want = bool(f_be(c.get("in") or ""))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH boundary in={c.get('in')!r} C={c.get('out')} PY={want}")
    if total == 0:
        print("ORACLE display_shell: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE display_shell: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
