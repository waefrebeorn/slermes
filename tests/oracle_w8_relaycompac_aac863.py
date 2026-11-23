#!/usr/bin/env python3
"""Differential oracle for agent/relay_compaction.py (w8_relaycompac_aac863 gate).

relay_runtime collaborators are injected from fixtures; LIVE defs are driven
with the same inputs and target projections compared to the C harness.
"""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

STATE = {"enabled": False, "turn": None, "host": None}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class RelayRuntime:
    pass


agent = _mod("agent")
agent.__path__ = []
rt = _mod("agent.relay_runtime", RelayRuntime=RelayRuntime, SCOPE_OP_TIMEOUT=5,
          runtime_metadata=lambda rid: {},
          relay_instrumentation_enabled=lambda: STATE["enabled"],
          active_turn=lambda sid: STATE["turn"])
rt.HOST_REGISTRY = types.SimpleNamespace(for_profile=lambda **k: STATE["host"])
agent.relay_runtime = rt

SRC = Q + "/agent/relay_compaction.py"
spec = importlib.util.spec_from_file_location("w8_relaycompac_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_relaycompac_live"] = live
spec.loader.exec_module(live)


def norm(v):
    if isinstance(v, (tuple, list)):
        return [norm(x) for x in v]
    if hasattr(v, "label") and not isinstance(v, (str, bytes, dict)):
        return v.label
    return v


def to_tuple(v):
    return tuple(v) if isinstance(v, list) else None


def norm_target(v):
    return norm(v) if v is not None else None


def build_turn(a):
    session = types.SimpleNamespace(handle=a.get("session_handle"), label="S")
    lease = types.SimpleNamespace(
        live_runtime=lambda: ("runtime" if a.get("live_runtime") else None),
        session=session)
    return types.SimpleNamespace(lease=lease, handle=a.get("turn_handle"))


def drive(r):
    func = r.get("func")
    sid = r.get("session_id", "s")
    if func == "turn_target":
        STATE["turn"] = build_turn(r) if r.get("active") else None
        return norm_target(live._turn_target(sid))
    if func == "session_target":
        if r.get("host_runtime"):
            host = RelayRuntime()
            host.label = "H"
        else:
            host = types.SimpleNamespace()
        session = (types.SimpleNamespace(handle=r.get("session_handle"), label="S")
                   if r.get("session_exists") else None)
        host.get_session = lambda s: session
        STATE["host"] = host
        return norm_target(live._session_target(sid))
    if func == "resolve_target":
        STATE["enabled"] = bool(r.get("enabled"))
        tsi = r.get("turn_session_id")

        def turn_stub(s):
            if tsi is not None and s == tsi:
                return to_tuple(r.get("turn_turn_target"))
            return to_tuple(r.get("turn_target"))

        live._turn_target = turn_stub
        live._session_target = lambda s: to_tuple(r.get("session_target"))
        return norm_target(live._resolve_target(sid, tsi))
    if func == "target_available":
        live._resolve_target = lambda s, turn_session_id=None: to_tuple(r.get("target"))
        return live.compaction_target_available(sid)
    if func == "emit_mark":
        tgt = to_tuple(r.get("target"))
        if tgt is not None:
            outcome = r.get("run_outcome", "ok")

            def run(*a, **k):
                if outcome == "timeout":
                    raise TimeoutError("stalled")
                if outcome == "error":
                    raise RuntimeError("boom")
            host = types.SimpleNamespace(
                run_in_session=run,
                relay=types.SimpleNamespace(scope=types.SimpleNamespace(event="ev")),
                runtime_id="rt")
            tgt = (host, types.SimpleNamespace(handle="h", label="S"),
                   tgt[2] if len(tgt) > 2 else "h")
        live._resolve_target = lambda s, turn_session_id=None: tgt
        live._stalled = bool(r.get("stalled"))
        try:
            return live.emit_compaction_mark(sid, r.get("name", "hermes.compaction"),
                                             r.get("data") or {})
        except BaseException as exc:
            return {"__raise__": type(exc).__name__}
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
    try:
        py = drive(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s: %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)

print("ORACLE w8_relaycompac_aac863: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
