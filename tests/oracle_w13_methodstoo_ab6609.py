#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_tools.py (w13_methodstoo_ab6609).

The full method module registers many handlers at import; stubbing method_ctx's
HandlerRegistry with identity decorators lets it load, and only three pure
helpers are exercised against the live module.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class HandlerRegistry:
    def method(self, name=None):
        def deco(fn):
            return fn
        return deco

    def profile_scoped(self, fn):
        return fn


tg = _mod("tui_gateway")
tg.__path__ = [Q + "/tui_gateway"]
_mod("tui_gateway.method_ctx", HandlerRegistry=HandlerRegistry, bind_module=lambda *a, **kw: None)

SRC = Q + "/tui_gateway/methods_tools.py"
spec = importlib.util.spec_from_file_location("tui_gateway.tools_w13_ab6609_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _result(stdout, stderr):
    return types.SimpleNamespace(stdout=stdout, stderr=stderr)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "str_arg":
            py = live._str_arg(r.get("params"), r.get("key"))
        elif func == "joined_output":
            py = live._joined_output(_result(r.get("stdout"), r.get("stderr")))
        elif func == "is_snapshot_restore":
            py = live._is_snapshot_restore(r.get("arg"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_methodstoo_ab6609: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
