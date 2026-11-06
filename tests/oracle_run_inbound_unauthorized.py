#!/usr/bin/env python3
"""Oracle for gateway/run_inbound_unauthorized.py::pairing_profile_arg.

Module imports stdlib + agent.i18n (t) + gateway.pairing — the pairing_store
attr access and the pure mapping are what we test. The live `pairing_profile_arg`
only touches `getattr(pairing_store, "profile", None)`; we import it directly
and drive it with a namespace carrying `profile`. profile:Null in the harness
emits as a non-attrs namespace -> getattr returns None -> "".
"""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
# gateway.run_inbound_unauthorized pulls in i18n/pairing; those are fine to
# import (stdlib) — but to avoid any heavy side effects, ast-extract the single
# live function instead.
import ast

SRC = "C:/Users/eman5/hermes-agent/gateway/run_inbound_unauthorized.py"


def main() -> int:
    ns = {}
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "pairing_profile_arg":
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), SRC, "exec"), ns)
    FN = ns["pairing_profile_arg"]

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
        print("ORACLE run_inbound_unauthorized: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "pair":
            continue
        total += 1
        store = types.SimpleNamespace()
        if c["profile"] is not None:
            store.profile = c["profile"]
        want = FN(store)
        if want != c["out"]:
            mism += 1
            print(f"MISMATCH {c.get('case')} profile={c['profile']!r}: "
                  f"C={c['out']!r} PY={want!r}")
    print(f"ORACLE run_inbound_unauthorized: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
