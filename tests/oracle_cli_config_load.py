#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_config_load.py pure helpers.

Harness (t_port_cli_config_load.c) emits per family:
  pst:   {family:"pst", in:<raw>, out:<str|null>}   -> _parse_service_tier_config(raw)
  merge: {family:"merge", defaults:<json-str>, file:<json-str>, out:<merged json-str>}
           -> deepcopy(defaults), _merge_file_config(dc, json.loads(file)), compare serialized

_parse_service_tier_config does `from agent.fast_mode import ...` INSIDE its body,
so inject the real agent.fast_mode module into sys.modules (inline-import seed).
_merge_file_config mutates defaults in place and returns None.
"""
import ast
import copy
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_config_load.py"
PYROOT = "C:/Users/eman5/hermes-agent"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def main() -> int:
    # inject a real agent.fast_mode so the inline import resolves
    sys.path.insert(0, PYROOT)
    try:
        import agent.fast_mode  # noqa: F401  (makes the inline import resolve)
    except Exception as e:
        print("ORACLE cli_config_load: agent.fast_mode import failed:", e)
        return 1

    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object,
          "logger": types.SimpleNamespace(warning=lambda *a, **k: None)}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in
            ("_parse_service_tier_config", "_merge_file_config")]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)

    total = 0
    mism = 0
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
        if fam == "pst":
            want = ns["_parse_service_tier_config"](c.get("in"))
            gotline = c.get("out")
            got = None if gotline is None else gotline
            cw = _canon(want)
            cg = _canon(got)
            if cw != cg:
                mism += 1
                print(f"MISMATCH pst in={c.get('in')!r} C={got!r} PY={want!r}")
        elif fam == "merge":
            # defaults is emitted as a raw JSON object (auto-decoded by json.loads);
            # file is jstr-quoted, so it arrives as a string to parse again.
            defs = c.get("defaults") if isinstance(c.get("defaults"), dict) else json.loads(c.get("defaults"))
            file = json.loads(c.get("file")) if isinstance(c.get("file"), str) else c.get("file")
            dc = copy.deepcopy(defs)
            ns["_merge_file_config"](dc, copy.deepcopy(file))
            want = json.dumps(dc, sort_keys=True)
            got = c.get("out")
            try:
                want = json.loads(want)
                got = json.loads(got)
            except Exception:
                pass
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH merge case={c.get('case')} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE cli_config_load: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE cli_config_load: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
