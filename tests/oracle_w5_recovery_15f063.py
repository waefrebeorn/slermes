#!/usr/bin/env python3
"""Oracle for the pm/recovery.py C port (gate w5_recovery_15f063).

The startup script is evaluated from the LIVE validate_environment source
(the STARTUP_IMPORTS tuple + the script-building f-string expression), so the
expected bytes come straight from quarry. Dependency normalisation uses the
same split/lower/replace expression the script applies.
"""
import ast
import json
import re
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "pm" / "recovery.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "pm" / "recovery.py")
sys.path.insert(0, str(QUARRY))

pm = sys.modules.get("pm")
if pm is None:
    pm = types.ModuleType("pm")
    pm.__path__ = [str(QUARRY / "pm")]
    sys.modules["pm"] = pm

tree = ast.parse(Path(SRC).read_text(encoding="utf-8"))
ns = {}
for n in tree.body:
    if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) and n.targets[0].id == "STARTUP_IMPORTS":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
SCRIPT_EXPR = None
for n in ast.walk(tree):
    if isinstance(n, ast.FunctionDef) and n.name == "validate_environment":
        for s in n.body:
            if isinstance(s, ast.Assign) and any(getattr(x, "id", None) == "script" for x in s.targets):
                SCRIPT_EXPR = s.value
LIVE_SCRIPT = eval(compile(ast.Expression(SCRIPT_EXPR), SRC, "eval"), dict(ns))
SPLIT_RE = re.compile(r"[\[<>=!~; @]")


def normalize(dep):
    return SPLIT_RE.split(dep, 1)[0].lower().replace("_", "-")


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
            if func == "w5_recovery_15f063_startup_script":
                exp = LIVE_SCRIPT
            elif func == "w5_recovery_15f063_normalize_dependency":
                exp = normalize(rec.get("dep", ""))
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
    print("ORACLE w5_recovery_15f063: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
