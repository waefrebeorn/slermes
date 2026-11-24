#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_injection.py (w16_pluginsinj_e56fea gate).

Replays the LIVE inject_plugin_message with instrumented collaborator objects:
queues/methods record the route and the message, and a handler on the live
logger turns each refusal warning into the corresponding route label.  The C
harness emits {"ok", "route", "msg", "target"}; this oracle rebuilds the same
four fields from the live function.
"""
import importlib.util
import json
import logging
import sys
import types
from collections.abc import Mapping
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/plugins_injection.py"
spec = importlib.util.spec_from_file_location("w16_pluginsinj_e56fea_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w16_pluginsinj_e56fea_live"] = live
spec.loader.exec_module(live)

# Function-local `from gateway.session_context import bound_session_key`.
# Parent-package bootstrap + registration so the live import hits the stub.
_gateway_pkg = types.ModuleType("gateway")
_gateway_pkg.__path__ = []
sys.modules["gateway"] = _gateway_pkg
_gw_ctx = types.ModuleType("gateway.session_context")
_call_key = [""]
_gw_ctx.bound_session_key = lambda: _call_key[0]
sys.modules["gateway.session_context"] = _gw_ctx


class _Warnings(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.WARNING)
        self.msgs = []

    def emit(self, record):
        self.msgs.append(record.getMessage())


_REFUSAL_ROUTES = (
    ("not both", "refused_both"),
    ("requires an existing session_key", "refused_keyless_no_gw"),
    ("origin must be a mapping", "refused_invalid_origin"),
    ("gateway injection denied", "refused_denied"),
    ("TUI scheduling failed", "refused_tui_exception"),
    ("no live gateway is available", "refused_no_gw"),
    ("gateway scheduling failed", "refused_gw_exception"),
)


def compute(r):
    content = r.get("content", "") or ""
    role = r.get("role", "user") or "user"
    plugin_id = r.get("plugin_id", "p") or "p"
    _call_key[0] = r.get("calling_session_key", "") or ""
    events = {"route": None, "target": None}
    warns = []
    handler = _Warnings()

    class _Queue:
        def __init__(self, name):
            self.name = name

        def put(self, message):
            events["route"] = "cli_interrupt" if self.name == "int" else "cli_pending"

    cli = None
    if r.get("has_cli"):
        cli = SimpleNamespace(
            _agent_running=bool(r.get("agent_running", False)),
            _interrupt_queue=_Queue("int"),
            _pending_input=_Queue("pend"),
        )

    def tui(**kw):
        events["route"] = "tui"
        events["target"] = None
        _ = (kw.get("content"), kw.get("plugin_id"))
        if r.get("tui_raises"):
            raise RuntimeError("TUI scheduling failed")
        return bool(r.get("tui_result", False))

    def gw(**kw):
        if kw.get("keyless"):
            events["route"] = "gw_keyless"
        elif "origin" in kw:
            events["route"] = "gw_origin"
        else:
            events["route"] = "gw_keyed"
        events["target"] = {k: v for k, v in kw.items()
                            if k not in ("content", "plugin_id")}
        _ = (kw.get("content"), kw.get("plugin_id"))
        if r.get("gw_raises"):
            raise RuntimeError("gateway scheduling failed")
        return bool(r.get("gw_result", False))

    manager = SimpleNamespace(
        _cli_ref=cli,
        has_gateway_message_injector=bool(r.get("has_gw_injector", False)),
        has_tui_message_injector=bool(r.get("has_tui_injector", False)),
        home_path=r.get("plugin_home", "") or "",
        inject_tui_message=tui,
        inject_gateway_message=gw,
    )
    ctx = SimpleNamespace(
        _manager=manager,
        plugin_id=plugin_id,
        _gateway_injection_allowed=lambda: bool(r.get("injection_allowed", False)),
    )

    old_level = live.logger.level
    live.logger.addHandler(handler)
    live.logger.setLevel(logging.WARNING)
    try:
        verdict = live.inject_plugin_message(
            ctx, content, role,
            session_key=(r.get("session_key") or None),
            origin=r.get("origin"),
        )
    finally:
        live.logger.removeHandler(handler)
        live.logger.setLevel(old_level)

    route = None
    for message in handler.msgs:
        for needle, label in _REFUSAL_ROUTES:
            if needle in message:
                route = label
                break
        if route:
            break
    if route is None:
        route = events["route"] or "none"
        target = events["target"]
    else:
        target = None

    msg = content if role == "user" else f"[{role}] {content}"
    return {"ok": bool(verdict), "route": route, "msg": msg, "target": target}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w16_pluginsinj_e56fea: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
