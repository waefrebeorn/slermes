#!/usr/bin/env python3
"""Differential oracle for tui_gateway/agent_factory.py (w13_agentfacto_542e88).

The module is a data-only helper file bound onto the TUI server at registration.
We bootstrap the tui_gateway package, stub method_ctx, and inject the two server
globals these pure helpers read (json, logger, _STATUSBAR_MODES).
"""
import importlib.util
import json
import logging
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

SRC = Q + "/tui_gateway/agent_factory.py"
spec = importlib.util.spec_from_file_location("tui_gateway.agent_w13_542e88_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

# Server-bound globals these helpers reference (values from tui_gateway/server.py).
live.json = json
live.logger = logging.getLogger("tui_gateway.agent_factory")
live._STATUSBAR_MODES = frozenset({"off", "top", "bottom"})

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
        if func == "parse_model_config":
            py = live._parse_model_config(r.get("raw"), quiet=bool(r.get("quiet", True)))
        elif func == "row_follows_profile":
            py = live._row_follows_profile(r.get("row"))
        elif func == "display_mouse_tracking":
            py = live._display_mouse_tracking(r.get("display"))
        elif func == "coerce_statusbar":
            py = live._coerce_statusbar(r.get("value"))
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

print(f"ORACLE w13_agentfacto_542e88: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
