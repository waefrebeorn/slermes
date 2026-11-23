#!/usr/bin/env python3
"""Oracle for the pm/registry.py C port (gate w5_registry_8fd451).

AST-extracts the LIVE walk() and drives it with a stub get_package() whose
.deps come from the fixture map. Compares the deps-first name order (or the
ValueError cycle marker) to the C harness output.
"""
import ast
import json
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
if not (QUARRY / "pm" / "registry.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "pm" / "registry.py")
sys.path.insert(0, str(QUARRY))

# parent-package bootstrap so LIVE imports that touch pm.* resolve in quarry
pm = sys.modules.get("pm")
if pm is None:
    pm = types.ModuleType("pm")
    pm.__path__ = [str(QUARRY / "pm")]
    sys.modules["pm"] = pm

src = Path(SRC).read_text(encoding="utf-8")
tree = ast.parse(src)
ns = {"__name__": "w5_registry_live", "__builtins__": __builtins__}
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name == "walk":
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
WALK = ns["walk"]

_DEPS = {}


def get_package(name):
    return types.SimpleNamespace(name=name, deps=list(_DEPS.get(name, [])))


WALK.__globals__["get_package"] = get_package


def norm_exception(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def main():
    global _DEPS
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        got = rec.get("out")
        _DEPS = rec.get("deps") or {}
        try:
            if func == "w5_registry_8fd451_walk":
                pkgs = WALK(list(rec.get("names") or []))
                exp = [p.name for p in pkgs]
            else:
                exp = {"__unknown__": func}
        except BaseException as e:
            exp = norm_exception(e)
        n += 1
        if got != exp:
            mism += 1
            if shown < 10:
                shown += 1
                print("MISMATCH %s case %d:\n  C=%s\n  P=%s" % (
                    func, n, json.dumps(got), json.dumps(exp)), file=sys.stderr)
    print("ORACLE w5_registry_8fd451: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
