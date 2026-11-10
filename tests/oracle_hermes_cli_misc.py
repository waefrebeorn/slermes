#!/usr/bin/env python3
"""Oracle for three thin pure helpers of:
  hermes_cli/auth_commands.py:_display_source
  agent/delegation_registry.py:configured_backend
  agent/context_compressor_telemetry.py:_safe_int

Harness (t_port_hermes_cli_misc.c) emits per family:
  displaysrc: {family:"displaysrc", in:<str|null>, out:<str>}
  backend:    {family:"backend", in:<config-json>, out:<str>}  (in already parsed)
  safeint:    {family:"safeint", in:<json-value>, ret:<int|-1>}

live _display_source: returns source.split(":",1)[1] if source.startswith("manual:") else source
live configured_backend: raw=config.get("backend") if Mapping else None;
     return lower_key(raw) if isinstance(raw,str) and raw.strip() else DEFAULT_BACKEND
     (lower_key = name.strip().lower()); DEFAULT_BACKEND="default"
live _safe_int: int(value) else None on TypeError/ValueError. C maps None->-1 and
     truncates floats (int(1.5)->1), bools->1/0.
We ast-extract each live fn verbatim and seed the module names it needs.
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def load(src_path, names, seeds=None):
    src = open(src_path, encoding="utf-8").read()
    ns = {"Any": object}
    if seeds:
        ns.update(seeds)
    wanted = set(names)
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in wanted:
            body.append(n)
    if body:
        exec(compile(ast.Module(body=body, type_ignores=[]), src_path, "exec"), ns)
    return ns


def main() -> int:
    fns = {}
    fns["displaysrc"] = load(
        "C:/Users/eman5/hermes-agent/hermes_cli/auth_commands.py",
        ["_display_source"])["_display_source"]
    reg = load("C:/Users/eman5/hermes-agent/agent/delegation_registry.py",
               ["configured_backend"],
               seeds={"DEFAULT_BACKEND": "default",
                      "Mapping": __import__("collections.abc", fromlist=["Mapping"]).Mapping,
                      "lower_key": (lambda s: s.strip().lower())})
    fns["backend"] = reg["configured_backend"]
    fns["safeint"] = load(
        "C:/Users/eman5/hermes-agent/agent/context_compressor_telemetry.py",
        ["_safe_int"])["_safe_int"]

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
        if fam == "displaysrc":
            # C treats a NULL buffer as the empty string; live would crash on None.
            # Short-circuit None to the empty-string result (C's NULL->"" convention).
            if c.get("in") is None:
                want = ""
            else:
                want = fns["displaysrc"](c.get("in"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH displaysrc in={c.get('in')!r} C={got!r} PY={want!r}")
        elif fam == "backend":
            want = fns["backend"](c.get("in"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH backend in={c.get('in')!r} C={got!r} PY={want!r}")
        elif fam == "safeint":
            try:
                wv = fns["safeint"](c.get("in"))
            except Exception:
                wv = None
            want = -1 if wv is None else int(wv)
            got = int(c.get("ret"))
            if want != got:
                mism += 1
                print(f"MISMATCH safeint in={c.get('in')!r} C={got} PY={want}")
    if total == 0:
        print("ORACLE hermes_cli_misc: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE hermes_cli_misc: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
