#!/usr/bin/env python3
"""Oracle for the pm/progress.py C port (gate w5_progress_88711f).

AST-extracts the LIVE verbose_output plus the _TRUE/_FALSE/_ANSI/MAX_LINE
constants from the quarry source; the ANSI regex and MAX_LINE come straight
from the live module so the projected cleaning/normalisation is faithful.
"""
import ast
import json
import os
import re
import sys
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "pm" / "progress.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "pm" / "progress.py")
sys.path.insert(0, str(QUARRY))

src = Path(SRC).read_text(encoding="utf-8")
tree = ast.parse(src)
ns = {"__name__": "w5_progress_live", "__builtins__": __builtins__,
      "re": re, "os": os}
WANT_CONST = {"_TRUE", "_FALSE", "_ANSI", "MAX_LINE", "TAIL_LINES", "_REDRAW_INTERVAL"}
for node in tree.body:
    if isinstance(node, ast.Assign):
        tg = node.targets[0]
        if isinstance(tg, ast.Name) and tg.id in WANT_CONST:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    elif isinstance(node, ast.FunctionDef) and node.name == "verbose_output":
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)

ANSI = ns["_ANSI"]
MAX_LINE = ns["MAX_LINE"]
VERBOSE = ns["verbose_output"]


def ansi_strip(line):
    return ANSI.sub("", line[-MAX_LINE:]).rstrip()


def normalize_lines(partial, text):
    s = (partial + text).replace("\r\n", "\n").replace("\r", "\n")
    lines = s.split("\n")
    partial_out = lines.pop()[-MAX_LINE:]
    return {"lines": lines, "partial": partial_out}


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        got = rec.get("out")
        try:
            if func == "w5_progress_88711f_verbose_output":
                exp = 1 if VERBOSE(rec.get("env") or {}) else 0
            elif func == "w5_progress_88711f_ansi_strip":
                exp = ansi_strip(rec.get("line", ""))
            elif func == "w5_progress_88711f_normalize_lines":
                exp = normalize_lines(rec.get("partial", ""), rec.get("text", ""))
            else:
                exp = {"__unknown__": func}
        except BaseException as e:
            exp = {"__raise__": type(e).__name__}
        n += 1
        if got != exp:
            mism += 1
            if shown < 10:
                shown += 1
                print("MISMATCH %s case %d:\n  C=%s\n  P=%s" % (
                    func, n, json.dumps(got), json.dumps(exp)), file=sys.stderr)
    print("ORACLE w5_progress_88711f: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
