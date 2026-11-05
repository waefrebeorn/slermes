#!/usr/bin/env python3
"""Oracle for cron/blueprint_plugins.py pure validators: _text, _strings,
_placeholders. The three defs are ast-extracted and run live; _placeholders
needs the module's `string`/`re`/`_SLOT_NAME_RE` in its namespace.
"""
import ast
import json
import re
import string
import sys
from typing import Iterable

SRC = "C:/Users/eman5/hermes-agent/cron/blueprint_plugins.py"
_SLOT_NAME_RE = re.compile(r"^[A-Za-z_]\w{0,63}$")


def extract(path, wants):
    ns = {"string": string, "re": re, "_SLOT_NAME_RE": _SLOT_NAME_RE,
          "Iterable": Iterable}
    tree = ast.parse(open(path, encoding="utf-8").read())
    funcs = []
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in wants:
            funcs.append(n)
    for n in funcs:
        exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return {f.name: ns[f.name] for f in funcs}


NS = extract(SRC, {"_text", "_strings", "_placeholders"})
_text = NS["_text"]
_strings = NS["_strings"]
_placeholders = NS["_placeholders"]


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
        print("ORACLE blueprint_plugins: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "text":
            try:
                want_v = _text("title", c["in"])
                if "err" in c:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C=ERR PY={want_v!r}")
                elif c["out"] != want_v:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want_v!r}")
            except ValueError:
                if "err" not in c:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C={c['out']!r} PY=<raise>")
        elif fam == "strings":
            value = json.loads(c["in"]) if c["in"] else None
            try:
                _strings("tags", value)
                py_ok = 1
            except ValueError:
                py_ok = 0
            if c["ok"] != py_ok:
                mism += 1
                print(f"MISMATCH {c['case']}: C ok={c['ok']} PY ok={py_ok} "
                      f"in={c['in']!r}")
        elif fam == "ph":
            try:
                want = _placeholders(c["in"], "prompt_template")
                if "err" in c:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C=ERR PY={want!r}")
                elif c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r}")
            except ValueError:
                if "err" not in c:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C={c.get('out')!r} PY=<raise>")
    print(f"ORACLE blueprint_plugins: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
