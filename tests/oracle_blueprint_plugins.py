#!/usr/bin/env python3
"""Oracle for cron/blueprint_plugins.py pure validators
(port include/port_blueprint_plugins.c). The cron package __init__ pulls heavy
deps (ruamel via cron.jobs->utils), so we ast-extract the three live fns +
_SLOT_NAME_RE straight from the module source and seed the namespace:
  text:     _text(name, value) -> stripped or ValueError
  strings:  _strings(name, value_iterable) -> tuple or ValueError
  placeholders: _placeholders(template) -> slot names or ValueError
C maps ValueError/non-zero to an error flag in the emit."""
import ast
import json
import re
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/cron/blueprint_plugins.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    g = {"Any": None, "Iterable": __import__("collections.abc", fromlist=["Iterable"]).Iterable,
         "string": __import__("string"), "re": re}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in ("_text", "_strings", "_placeholders"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "bp", "exec"), g)
        elif isinstance(n, ast.Assign):
            for tar in n.targets:
                if isinstance(tar, ast.Name) and tar.id == "_SLOT_NAME_RE":
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "bp", "exec"), g)
    # _SLOT_NAME_RE may be AnnAssign; if still missing, re-extract by regex
    if "_SLOT_NAME_RE" not in g:
        m = re.search(r"_SLOT_NAME_RE\s*=\s*re\.compile\((r?\"[^\"]*\")\)", src)
        if m:
            g["_SLOT_NAME_RE"] = re.compile(eval(m.group(1)))
    g["_placeholders"].__globals__.update(g)
    f_text, f_strings, f_ph = g["_text"], g["_strings"], g["_placeholders"]

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
        if fam == "text":
            try:
                want = f_text("title", c.get("in"))
                if c.get("err"):
                    mism += 1
                    print(f"MISMATCH text case{c.get('case')} live ok but C err")
                elif want != c.get("out"):
                    mism += 1
                    print(f"MISMATCH text case{c.get('case')} C={c.get('out')!r} PY={want!r}")
            except ValueError:
                if not c.get("err"):
                    mism += 1
                    print(f"MISMATCH text case{c.get('case')} live ValueError but C ok")
        elif fam == "strings":
            try:
                val = c.get("in")
                if isinstance(val, str):
                    val = json.loads(val)
                f_strings("tags", val)
                if not c.get("ok"):
                    mism += 1
                    print(f"MISMATCH strings case{c.get('case')} live ok but C err")
            except ValueError:
                if c.get("ok"):
                    mism += 1
                    print(f"MISMATCH strings case{c.get('case')} live ValueError but C ok")
        elif fam == "placeholders":
            try:
                names = f_ph(c.get("in"), "x")
                if c.get("err"):
                    mism += 1
                    print(f"MISMATCH placeholders case{c.get('case')} live ok but C err")
                elif names != c.get("out"):
                    mism += 1
                    print(f"MISMATCH placeholders case{c.get('case')} C={c.get('out')!r} PY={names!r}")
            except ValueError:
                if not c.get("err"):
                    mism += 1
                    print(f"MISMATCH placeholders case{c.get('case')} live ValueError but C ok")
    if total == 0:
        print("ORACLE blueprint_plugins: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE blueprint_plugins: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
