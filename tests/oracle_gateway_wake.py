#!/usr/bin/env python3
"""Oracle for gateway/wake.py pure shape helpers.

Harness (t_port_gateway_wake.c) emits per family:
  push:     {family:"push", case, has, val, ret:<0|1>}
              adapter_supports_push = bool(getattr(adapter,"supports_async_delivery",True))
  draining: {family:"draining", case, ret:<0|1>}
              _runner_draining = getattr(getattr(adapter,"gateway_runner",None),"_draining",False) is True
  metadata: {family:"metadata", case, in:<evt-json>, w:<0|1>, out:<json|null>}
              _delegation_display_metadata(evt); w = whether
              warning_notifications_enabled("api_server") returned true, which the
              live fn imports INLINE -> inject that module on sys.modules and
              toggle its return per the case's w flag.
Compare metadata canonically (numbers float-coerced for libjson int rendering).
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/wake.py"


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, list):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in
            ("adapter_supports_push", "_runner_draining", "_delegation_display_metadata")]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)

    warn_mod = types.ModuleType("gateway.warning_notifications")
    gpkg = types.ModuleType("gateway")
    gpkg.warning_notifications = warn_mod
    sys.modules["gateway.warning_notifications"] = warn_mod

    class Adapter:
        pass

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
        if fam == "push":
            a = Adapter()
            if c.get("has"):
                a.supports_async_delivery = bool(c.get("val"))
            want = 1 if ns["adapter_supports_push"](a) else 0
            got = int(c.get("ret"))
            if want != got:
                mism += 1
                print(f"MISMATCH push case={c.get('case')} C={got} PY={want}")
        elif fam == "draining":
            dcase = str(c.get("case"))
            table = {"d_0": (0, 1, 1), "d_1": (1, 0, 1),
                     "d_2": (1, 1, 1), "d_3": (1, 1, 0)}
            run, present, draining = table.get(dcase, (0, 0, 0))
            a = Adapter()
            if run:
                runner = Adapter()
                if present:
                    runner._draining = bool(draining)
                a.gateway_runner = runner
            want = 1 if ns["_runner_draining"](a) else 0
            got = int(c.get("ret"))
            if want != got:
                mism += 1
                print(f"MISMATCH draining case={dcase} C={got} PY={want}")
        elif fam == "metadata":
            from collections.abc import Mapping as _Mapping
            evt = c.get("in")
            evt = evt if isinstance(evt, dict) else {}
            w_enabled = int(c.get("w")) == 1
            warn_mod.warning_notifications_enabled = (lambda _ctx, _we=w_enabled: _we)
            want = ns["_delegation_display_metadata"](evt)
            got_raw = c.get("out")
            got = None if got_raw is None else got_raw
            if isinstance(got, str):
                try:
                    got = json.loads(got)
                except ValueError:
                    got = got
            if _canon(want) != _canon(got):
                mism += 1
                print(f"MISMATCH metadata case={c.get('case')} w={c.get('w')}\n"
                      f"  C={got!r}\n  PY={want!r}")
    if total == 0:
        print("ORACLE gateway_wake: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE gateway_wake: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
