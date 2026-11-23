#!/usr/bin/env python3
"""Differential oracle for agent/micro_compaction.py (w8_microcompac_666e99 gate).

context_compressor constants and the host's _is_context_summary_message are
injected from fixtures; LIVE defs are driven with the same inputs.
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


agent = _mod("agent")
agent.__path__ = []
_mod("agent.compaction_events", publish_micro=lambda *a, **k: None)
_mod("agent.context_compressor_telemetry", _safe_int=lambda v: v)
sr = _mod("session_rules")
sr.__path__ = []
_mod("session_rules.turns",
     record_absorbed_message=lambda *a, **k: None,
     drop_stale_api_content=lambda *a, **k: None)
_mod("agent.model_metadata",
     estimate_messages_tokens_rough=lambda m: 0, estimate_tokens_rough=lambda s: 0)
ccstub = _mod("agent.context_compressor")
agent.context_compressor = ccstub

SRC = Q + "/agent/micro_compaction.py"
spec = importlib.util.spec_from_file_location("w8_microcompac_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_microcompac_live"] = live
spec.loader.exec_module(live)
Mixin = live.MicroCompactionMixin

MAP = [("summary_key", "COMPRESSED_SUMMARY_METADATA_KEY"),
       ("micro_key", "MICRO_COMPACT_MARKER_KEY"),
       ("summary_prefix", "SUMMARY_PREFIX"),
       ("heading", "HISTORICAL_TASK_HEADING"),
       ("end_marker", "_SUMMARY_END_MARKER"),
       ("max_failures", "_MICRO_COMPACT_MAX_CONSECUTIVE_FAILURES")]


def set_cc(r):
    cc = r.get("cc") or {}
    for src, dst in MAP:
        setattr(ccstub, dst, cc.get(src))


def make_obj(state):
    o = Mixin.__new__(Mixin)
    st = state or {}
    o._micro_compact_consecutive_failures = st.get("_micro_compact_consecutive_failures", 0)
    o._micro_compact_last_failure_cursor = st.get("_micro_compact_last_failure_cursor", -1)
    o._micro_compact_cursor = st.get("_micro_compact_cursor", 0)
    o._is_context_summary_message = lambda m: bool(m.get("_summary"))
    return o


def state_of(o):
    return {"_micro_compact_consecutive_failures": o._micro_compact_consecutive_failures,
            "_micro_compact_last_failure_cursor": o._micro_compact_last_failure_cursor,
            "_micro_compact_cursor": o._micro_compact_cursor}


def drive(r):
    func = r.get("func")
    set_cc(r)
    if func == "summary_marker":
        return live._is_summary_marker(r.get("entry"))
    if func == "micro_marker":
        return live._is_micro_marker(r.get("entry"))
    if func == "rolling":
        return Mixin._rolling_summary_from_marker(r.get("content"))
    if func == "render":
        return Mixin._render_micro_marker_content(r.get("summary_text"))
    if func == "record_failure":
        o = make_obj(r.get("state"))
        outcome = Mixin._record_micro_failure(o, r.get("exchange_start", 0), r.get("exchange_end", 0))
        return {"outcome": outcome, "state": state_of(o)}
    if func == "reset_failure":
        o = make_obj(r.get("state"))
        Mixin._reset_micro_failure_tracking(o)
        return state_of(o)
    if func == "find_exchange":
        o = make_obj(None)
        res = Mixin._find_one_exchange(o, r.get("messages") or [], r.get("start", 0), r.get("tail_start", 0))
        return list(res) if res is not None else None
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

print("ORACLE w8_microcompac_666e99: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
