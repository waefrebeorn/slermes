#!/usr/bin/env python3
"""Differential oracle for Hermes tui_gateway/change_watcher.py (gate w14_changewatc_e37815).

Stubs: tui_gateway.method_ctx (relative import at module load),
hermes_cli.skin_engine (init/get_active_skin), tui_gateway.event_replay
(replay_epoch). The parent tui_gateway package is bootstrapped with __path__
so spec_from_file_location executes the live module with its relative import.
Active skin and epoch are injected per fixture; the live process-wide counter
and the C static counter both advance once per groups_changed case.
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


tui = _mod("tui_gateway")
tui.__path__ = [Q + "/tui_gateway"]


class HandlerRegistry:
    pass


_mod("tui_gateway.method_ctx",
     HandlerRegistry=HandlerRegistry,
     bind_module=lambda *a, **k: None)

_epoch = {"v": ""}
_mod("tui_gateway.event_replay", replay_epoch=lambda: _epoch["v"])

hcli = _mod("hermes_cli")
hcli.__path__ = []
_skin = {"v": {}}


class _ActiveSkin:
    pass


def _get_active_skin():
    s = _ActiveSkin()
    for k, v in _skin["v"].items():
        setattr(s, k, v)
    return s


_mod("hermes_cli.skin_engine",
     init_skin_from_config=lambda cfg: None,
     get_active_skin=_get_active_skin)

SRC = Q + "/tui_gateway/change_watcher.py"
# Dotted module name so the live module's `from .method_ctx import ...` resolves.
spec = importlib.util.spec_from_file_location("tui_gateway.change_watcher", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.change_watcher"] = live
spec.loader.exec_module(live)

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
        if func == "skin_payload":
            _skin["v"] = r.get("skin") or {}
            py = live.skin_payload(r.get("config", {}))
        elif func == "groups_changed":
            _epoch["v"] = r.get("epoch", "")
            py = live._groups_changed_payload(r.get("before", {}), r.get("after", {}))
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

print(f"ORACLE w14_changewatc_e37815: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
