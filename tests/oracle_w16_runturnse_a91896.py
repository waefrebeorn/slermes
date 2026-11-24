#!/usr/bin/env python3
"""Differential oracle for gateway/run_turn_setup_guard.py (gate w16_runturnse_a91896)."""
import importlib.util
import json
import os
import sys
import time
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_runturnse_a91896"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_NOW = [0.0]
_ALLOWED = [False]
_TIMEOUT = [300.0]
_SENTINEL = object()


class _Dummy(types.ModuleType):
    def __getattr__(self, n):
        v = lambda *a, **k: None
        setattr(self, n, v)
        return v


def _boot(full):
    parts = full.split(".")
    for i in range(1, len(parts) + 1):
        nm = ".".join(parts[:i])
        if nm not in sys.modules:
            m = _Dummy(nm)
            if i < len(parts):
                m.__path__ = []
            sys.modules[nm] = m
    if len(parts) > 1:
        setattr(sys.modules[".".join(parts[:-1])], parts[-1], sys.modules[full])


_boot("agent.i18n")
_boot("gateway.run")
_boot("gateway.bot_peer_notices")
sys.modules["agent.i18n"].t = lambda key, **kw: "wait:%d" % kw["minutes"]
sys.modules["gateway.run"]._AGENT_PENDING_SENTINEL = _SENTINEL
sys.modules["gateway.run"]._float_env = lambda name, default: _TIMEOUT[0]
sys.modules["gateway.bot_peer_notices"].automatic_notices_allowed = lambda source: _ALLOWED[0]

_spec = importlib.util.spec_from_file_location(
    "gateway.run_turn_setup_guard_live", QUARRY + "/gateway/run_turn_setup_guard.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)

_real_time = time.time
time.time = lambda: _NOW[0]


class _Turn:
    pass


class _State:
    pass


class _Runner:
    def __init__(self, state):
        self._state = state

    def _peek_session_state(self, session_key):
        return self._state


def _compute(r):
    _NOW[0] = float(r.get("now", 0.0))
    _ALLOWED[0] = bool(r.get("allowed", False))
    _TIMEOUT[0] = float(r.get("timeout", 300.0))
    os.environ["HERMES_GATEWAY_BUSY_ACK_ENABLED"] = "true" if r.get("enabled") else "false"
    state = None
    if not r.get("state_is_none", False):
        state = _State()
        state.turn = _Turn()
        state.turn.started_ts = float(r.get("started", 0.0))
        state.turn.agent = _SENTINEL if r.get("agent_is_pending") else object()
        state.turn.busy_ack_ts = float(r.get("busy_ack_ts", 0.0))
    return live.pending_setup_busy_notice(_Runner(state), "session-key", object())


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {k: _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "{" not in line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = _compute(r)
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
time.time = _real_time
print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
