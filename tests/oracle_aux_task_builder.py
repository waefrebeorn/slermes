#!/usr/bin/env python3
"""Oracle for hermes_cli/plugins_aux_tasks.py build_auxiliary_task_entry
(port include/port_aux_task_builder.c:bat_aux_task_entry). The live fn raises
ValueError for invalid keys (port returns NULL) and inline-imports the built-in
aux-task key set from hermes_cli.main_provider_setup; the port takes that key set
as a param. Ast-extract the live fn, seed a fake sys.modules
hermes_cli.main_provider_setup._AUX_TASKS from the harness `bi` array, drive it
on the harness facts, and compare the rebuilt entry against the C `out` JSON."""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _arr(v):
    return v if isinstance(v, list) else json.loads(v)


def _obj(v):
    if v is None or v == "":
        return None if v == "" else None
    return v if isinstance(v, dict) else json.loads(v)


def _empty(v):
    return v in (None, "")


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/hermes_cli/plugins_aux_tasks.py",
               encoding="utf-8").read()
    g = {"Any": None, "Dict": None, "Optional": None, "logging": __import__("logging"),
         "logger": types.SimpleNamespace(warning=lambda *a, **k: None)}
    got = None
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "build_auxiliary_task_entry":
            exec(compile(ast.Module(body=[n], type_ignores=[]), "atb", "exec"), g)
            got = True
    if not got:
        print("ORACLE aux_task_builder: live fn not extracted")
        return 1
    build = g["build_auxiliary_task_entry"]

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
        # wire the inline import: hermes_cli.main_provider_setup._AUX_TASKS
        # as triples whose first element is the harness `bi` key.
        bi_keys = _arr(c["bi"])
        src3 = types.ModuleType("hermes_cli.main_provider_setup")
        src3._AUX_TASKS = [(k, "", "") for k in bi_keys]
        parent = types.ModuleType("hermes_cli")
        parent.__path__ = []
        sys.modules.setdefault("hermes_cli", parent)
        sys.modules["hermes_cli.main_provider_setup"] = src3

        try:
            want = build(
                c["me"], c["owner"], c["key"],
                display_name=c["dn"], description=c["desc"],
                defaults=_obj(c.get("defaults")) if not _empty(c.get("defaults")) else None,
                inherit_from=c.get("inh") if not _empty(c.get("inh")) else None,
                registered=_obj(c.get("reg")) if not _empty(c.get("reg")) else {},
            )
        except ValueError:
            want = None
        want_s = "null" if want is None else json.dumps(want, ensure_ascii=False, sort_keys=True)
        c_out = c.get("out")
        # compare on parsed value: harness embeds `out` as raw JSON (dict) — for a
        # NULL result the harness printed the literal "null", which parses to None.
        want_p = None if want is None else json.loads(want_s)
        if (want_p is None) != (c_out is None):
            mism += 1
            print(f"MISMATCH case{c.get('iwas') or c.get('case')} key={c.get('key')!r} C={c_out} PY={want_p}")
        elif want_p is not None and want_p != c_out:
            mism += 1
            print(f"MISMATCH case{c.get('iwas') or c.get('case')} key={c.get('key')!r} C={c_out} PY={want_p}")
    if total == 0:
        print("ORACLE aux_task_builder: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE aux_task_builder: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
