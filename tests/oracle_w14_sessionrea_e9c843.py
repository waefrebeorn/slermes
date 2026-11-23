#!/usr/bin/env python3
"""Differential oracle for w14_sessionrea_e9c843 (tui_gateway/session_reaper.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
S = Q + "/tui_gateway/session_reaper.py"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [Q + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg
_mc = types.ModuleType("tui_gateway.method_ctx")
_mc.bind_module = lambda *a, **kw: None
sys.modules["tui_gateway.method_ctx"] = _mc
_em = types.ModuleType("tui_gateway._env")
_em.env_float = lambda k, d: d
sys.modules["tui_gateway._env"] = _em

spec = importlib.util.spec_from_file_location("tui_gateway.session_reaper_w14", S)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

DETACHED = object()
class FanoutTransport:
    def __init__(self, peers): self._peers = peers
    def transports(self): return self._peers
live._detached_ws_transport = DETACHED
live.FanoutTransport = FanoutTransport

def conv_transport(t):
    if not isinstance(t, dict): return None
    if t.get("kind") == "detached": return DETACHED
    if t.get("kind") == "fanout":
        return FanoutTransport([conv_transport(p) for p in (t.get("peers") or [])])
    ns = types.SimpleNamespace()
    if "closed" in t: ns._closed = t["closed"]
    return ns

def conv_session(d):
    s = dict(d or {})
    ar = s.get("agent_ready")
    if isinstance(ar, dict):
        v = bool(ar.get("set"))
        s["agent_ready"] = types.SimpleNamespace(is_set=lambda: v)
    else:
        s["agent_ready"] = None
    s["transport"] = conv_transport(s.get("transport"))
    ag = s.get("agent")
    if isinstance(ag, dict):
        ns = types.SimpleNamespace()
        for k, v in ag.items(): setattr(ns, k, v)
        s["agent"] = ns
    return s

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    fl = r.get("flags") or {}
    try:
        live._session_pending_kind = lambda sid: bool(fl.get("pending_kind", False))
        live._session_has_active_delegations = lambda sid, s: bool(fl.get("active_delegations", False))
        live._session_owns_live_wakeup_schedule = lambda s: bool(fl.get("live_wakeup", False))
        live._session_owns_pending_process_notification = lambda s: bool(fl.get("pending_process_notification", False))
        session = conv_session(r.get("session"))
        if func == "session_has_pending_flush":
            py = live._session_has_pending_flush(session)
        elif func == "session_is_lru_evictable":
            py = live._session_is_lru_evictable(r.get("sid", "s"), session)
        elif func == "session_is_reapable":
            py = live._session_is_reapable(r.get("sid", "s"), session)
        elif func == "session_is_evictable":
            live._SESSION_TTL_S = float(r.get("ttl", 0))
            py = live._session_is_evictable(r.get("sid", "s"), session, float(r.get("now", 0)))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1; continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_sessionrea_e9c843: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
