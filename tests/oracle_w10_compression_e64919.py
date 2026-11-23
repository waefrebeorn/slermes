#!/usr/bin/env python3
"""Differential oracle for live agent/compression_facade.py (w10_compression_e64919).

Session-activity / conversation-compression / timeout-bookkeeping collaborators
are stubbed; the exports cover the lazy prompt fallback and the two emitted
warning strings (the agent/fence/threaded _compress_context wrapper stays host-side).
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


agent_pkg = _mod("agent")
agent_pkg.__path__ = []
_mod("agent.session_activity",
     ActivityProvenance=types.SimpleNamespace(AGENT_COMPRESSION_TIMEOUT="agent_compression_timeout"))
_mod("agent.conversation_compression", mark_context_compression_timed_out=lambda agent: None)

SRC = Q + "/agent/compression_facade.py"
spec = importlib.util.spec_from_file_location("cf_e64919_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cf_e64919_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "timeout_fallback_prompt":
        build_ok = bool(r.get("build_ok", False))
        if build_ok:
            def build(sm):
                return r.get("built", "")
        else:
            def build(sm):
                raise RuntimeError("builder failed")
        agent = types.SimpleNamespace(
            _cached_system_prompt=r.get("cached"),
            _build_system_prompt=build)
        return live._timeout_fallback_prompt(agent, r.get("system_message", ""))
    if f == "warn_commit_overrun":
        seen = []
        agent = types.SimpleNamespace(_emit_warning=lambda msg: seen.append(msg))
        live._warn_commit_overrun(agent, r.get("waited", 0.0), r.get("ceiling", 0.0))
        return seen[0] if seen else ""
    if f == "compression_timeout_warning":
        seen = []
        agent = types.SimpleNamespace(
            _touch_activity=None,
            context_compressor=types.SimpleNamespace(record_timeout_failure=lambda *a, **k: None),
            _emit_warning=lambda msg: seen.append(msg))
        live._report_compression_timeout(
            agent, idle=r.get("idle", 0.0), waited=r.get("waited", 0.0),
            since_progress=r.get("since_progress", 0.0), total_ceiling=r.get("total_ceiling", 0.0),
            total_exhausted=bool(r.get("total_exhausted", False)),
            progress_observed=bool(r.get("progress_observed", False)))
        return seen[0] if seen else ""
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
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w10_compression_e64919: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
