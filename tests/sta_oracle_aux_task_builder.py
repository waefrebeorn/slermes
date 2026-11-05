#!/usr/bin/env python3
"""Oracle for hermes_cli/plugins_aux_tasks.py build_auxiliary_task_entry.
The function is ast-extracted (it raises ValueError, so a None C output must
match a Python exception) and run live; the builtin-_AUX_TASKS and logger are
seeded.
"""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/plugins_aux_tasks.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

seg = None
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "build_auxiliary_task_entry":
        seg = ast.get_source_segment(code, n)
NS = {"logger": types.SimpleNamespace(warning=lambda *a, **k: None)}
exec(compile(ast.parse(seg), "x", "exec"), NS)
fn = NS["build_auxiliary_task_entry"]


def seed_builtin(keys):
    m = types.ModuleType("hermes_cli.main_provider_setup")
    m._AUX_TASKS = [(k, "", "") for k in keys]
    pkg = types.ModuleType("hermes_cli")
    sys.modules["hermes_cli"] = pkg
    sys.modules["hermes_cli.main_provider_setup"] = m


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
        # our lines carry the args raw; we reparse under the 'c' keys.
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE aux_task_builder: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        seed_builtin(c["bi"])
        try:
            want = fn(c["me"], c["owner"], c["key"], display_name=c["dn"],
                      description=c["desc"], defaults=c["defaults"],
                      inherit_from=c["inh"], registered=c["reg"])
            want_exc = False
        except ValueError:
            want_exc = True
            want = None
        got = c["out"]
        if want_exc:
            if got is not None:
                mism += 1
                print(f"MISMATCH case key={c['key']!r}: C={got} PY=ValueError")
        else:
            if got != want:
                mism += 1
                print(f"MISMATCH case key={c['key']!r}: C={got} PY={want}")
    print(f"ORACLE aux_task_builder: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
