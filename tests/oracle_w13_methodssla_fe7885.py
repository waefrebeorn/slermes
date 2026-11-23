#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_slash.py (w13_methodssla_fe7885).

The model-output formatter is exercised. The module is bound onto server.py at
registration, so we bootstrap the tui_gateway package and stub method_ctx;
the formatter receives a SimpleNamespace stand-in for session["agent"].
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

SRC = Q + "/tui_gateway/methods_slash.py"
spec = importlib.util.spec_from_file_location("tui_gateway.slash_w13_fe7885_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _agent(d):
    if isinstance(d, dict):
        return types.SimpleNamespace(**d)
    return None


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
        if func == "model_output":
            py = live._format_live_model_output({"agent": _agent(r.get("agent"))})
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

print(f"ORACLE w13_methodssla_fe7885: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
