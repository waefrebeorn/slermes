#!/usr/bin/env python3
"""Differential oracle for agent/conversation_compression_codex.py (w20_conversati_9e4fef).

The live module is imported from the quarry; the facade it late-imports, the
codex_runtime bookkeeping, the telemetry sink, the logger, the heartbeat and the
agent itself are all stubbed from the fixture.  Each harness line therefore
executes the real ``_compress_context_via_codex_app_server`` and the comparison
covers its decision, its log lines and every observable side effect.
"""
import contextlib
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

CAPTURED = {}

# ---- stub the modules the function reaches for -----------------------------
facade = types.ModuleType("agent.conversation_compression")
facade.COMPACTION_STATUS = "compaction_status"


class _Heartbeat:
    def __init__(self, agent, emit_client_status=True):
        self.agent = agent

    def start(self):
        return self

    def stop(self, label):
        CAPTURED["heartbeat"] = label


facade._CompressionActivityHeartbeat = _Heartbeat
facade._emit_compaction_done = lambda agent: None
facade._existing_system_prompt = lambda agent, sm: ("prompt", sm)
facade._reset_read_dedup_caches = lambda task_id, session_id="": CAPTURED.__setitem__(
    "dedup", {"task_id": task_id, "session_id": session_id})
facade._swallow = lambda *a, **k: contextlib.suppress(Exception)
sys.modules["agent.conversation_compression"] = facade

compressor_mod = types.ModuleType("agent.context_compressor")
compressor_mod._SUMMARY_FAILURE_COOLDOWN_SECONDS = 600
sys.modules["agent.context_compressor"] = compressor_mod

codex_runtime = types.ModuleType("agent.codex_runtime")
codex_runtime._record_codex_app_server_compaction = lambda *a, **k: CAPTURED.__setitem__(
    "bookkeeping", True)
codex_runtime._record_codex_app_server_usage = lambda *a, **k: CAPTURED.__setitem__("usage", True)
sys.modules["agent.codex_runtime"] = codex_runtime

NAME = "w20_conversati_9e4fef_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "conversation_compression_codex.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class FakeLogger:
    def __init__(self):
        self.lines = []

    def info(self, fmt, *args):
        self.lines.append(fmt % args)

    def debug(self, *a, **k):
        pass


def capture_telemetry(agent, started_at, *, commit_status, failure_class, approx_tokens,
                      route="hermes", method="none", attempt_seed=None):
    CAPTURED["record"] = {"commit_status": commit_status, "failure_class": failure_class,
                          "route": route, "method": method}


live._emit_bypassed_attempt_telemetry = capture_telemetry


def as_result(spec):
    """A compact_thread result object: real attributes, no close() of its own."""
    return types.SimpleNamespace(**spec)


def run_plan(r):
    CAPTURED.clear()
    comp = types.SimpleNamespace()
    if r.get("cooldown_recorder"):
        def recorder(secs, err):
            CAPTURED["cooldown"] = [secs, err]
        comp._record_compression_failure_cooldown = recorder
    cooldown_value = r.get("cooldown", 0.0)

    def getter(refresh=False):
        return {"remaining_seconds": cooldown_value}

    comp.get_active_compression_failure_cooldown = getter
    if r.get("has_update_from_response"):
        comp.update_from_response = lambda *a, **k: None

    session = None
    if r.get("has_session"):
        spec = r.get("result")
        if isinstance(spec, dict):
            result = as_result(spec)
            session = types.SimpleNamespace(compact_thread=lambda: result)
        else:
            exc_type = type(r.get("raise_type") or "RuntimeError", (Exception,), {})

            def compact():
                raise exc_type("boom")

            session = types.SimpleNamespace(compact_thread=compact)
        session.close = lambda: CAPTURED.__setitem__("closed", True)

    agent = types.SimpleNamespace(
        session_id=r.get("session_id"),
        context_compressor=comp,
        codex_app_server_auto_compaction=r.get("auto_mode", "native"),
        _codex_session=session,
    )
    agent._emit_status = lambda s: None
    agent._emit_warning = lambda m: CAPTURED.__setitem__("warning", m)
    logger = FakeLogger()
    live.logger = logger

    try:
        live._compress_context_via_codex_app_server(
            agent, r.get("messages") or [], None,
            approx_tokens=r.get("approx_tokens"), task_id=r.get("task_id", "default"),
            force=bool(r.get("force")), started_at=0.0, attempt_seed={})
    except BaseException as exc:  # noqa: BLE001 - the raise path is part of the contract
        CAPTURED["exception"] = type(exc).__name__

    tokens = "unknown"
    auto_mode = "native"
    for line in logger.lines:
        if "tokens=~" in line:
            tokens = line.split("tokens=~", 1)[1].rstrip(")")
        if "compaction skipped: mode=" in line:
            auto_mode = line.split("compaction skipped: mode=", 1)[1].split(" force=false", 1)[0]
    if logger.lines and "no active codex thread" in logger.lines[0]:
        auto_mode = "hermes"      # only `mode=hermes` gets past the first gate
    retired = bool(CAPTURED.get("closed"))
    return {
        "auto_mode": auto_mode,
        "session_id": str(r.get("session_id") or "none"),
        "tokens": tokens,
        "logs": logger.lines,
        "record": CAPTURED.get("record"),
        "heartbeat_stop": CAPTURED.get("heartbeat"),
        "warning": CAPTURED.get("warning"),
        "cooldown_record": CAPTURED.get("cooldown"),
        "retired": retired,
        "closed": retired,
        "bookkeeping": bool(CAPTURED.get("bookkeeping")),
        "usage": bool(CAPTURED.get("usage")),
        "dedup_reset": CAPTURED.get("dedup"),
        "exception": CAPTURED.get("exception"),
    }


def live_skip(inp):
    """The skipper's verdict, read off a real call's log line + telemetry record."""
    p = run_plan({"force": bool(inp.get("force")),
                  "auto_mode": inp.get("auto_mode", "native"),
                  "cooldown": inp.get("cooldown", 0.0),
                  "has_session": bool(inp.get("has_session")),
                  "result": {}, "session_id": "s"})
    first = p["logs"][0] if p["logs"] else ""
    if "compaction skipped: " not in first:
        return None
    reason = first.split("compaction skipped: ", 1)[1].split(" (session=", 1)[0]
    return {"reason": reason, "status": p["record"]["commit_status"],
            "class": p["record"]["failure_class"]}


def live_mode(raw):
    """The normalised auto mode, observed through the skipper's log line."""
    v = live_skip({"force": False, "auto_mode": raw or "native", "has_session": False})
    if v and "mode=" in v["reason"]:
        return v["reason"].split("mode=", 1)[1].split(" force=false", 1)[0]
    return "hermes"           # only `mode=hermes` gets past the first gate


def cooldown_agent(spec):
    if not isinstance(spec, dict):
        return None
    comp = spec.get("compressor")
    if not isinstance(comp, dict):
        return types.SimpleNamespace(context_compressor=None)
    obj = types.SimpleNamespace()
    if comp.get("has_getter"):
        if comp.get("raises"):
            def getter(refresh=False):
                raise RuntimeError("boom")
        else:
            state = comp.get("state")

            def getter(refresh=False):
                return state
        obj.get_active_compression_failure_cooldown = getter
    obj._state = comp.get("state")
    return types.SimpleNamespace(context_compressor=obj)


def expected(r):
    f = r.get("func")
    if f == "auto_mode":
        return live_mode(r.get("raw"))
    if f == "tokens_label":
        return run_plan({"force": False, "auto_mode": "native",
                         "approx_tokens": r.get("approx_tokens")})["tokens"]
    if f == "cooldown_remaining":
        return live._codex_compaction_cooldown_remaining(cooldown_agent(r.get("agent")))
    if f == "skip_decision":
        return live_skip(r)
    if f == "failed":
        res = r.get("result")
        if not isinstance(res, dict):
            return False
        return run_plan({"force": True, "auto_mode": "native", "has_session": True,
                         "result": {"interrupted": res.get("interrupted"),
                                    "error": res.get("error")}})["heartbeat_stop"] \
            == "context compression failed"
    if f == "failure_message":
        res = r.get("result") if isinstance(r.get("result"), dict) else {}
        return run_plan({"force": True, "auto_mode": "native", "has_session": True,
                         "result": {"interrupted": True, "error": res.get("error")}})["warning"]
    if f == "failure_error":
        res = r.get("result") if isinstance(r.get("result"), dict) else {}
        out = run_plan({"force": True, "auto_mode": "native", "has_session": True,
                        "cooldown_recorder": True,
                        "result": {"interrupted": True, "error": res.get("error")}})
        return out["cooldown_record"][1] if out["cooldown_record"] else None
    if f == "status_text":
        return run_plan({"force": True, "auto_mode": "native", "has_session": True,
                         "result": {"interrupted": bool(r.get("failed"))}})["heartbeat_stop"]
    if f == "plan":
        p = run_plan(r)
        p["auto_mode"] = live_mode(r.get("auto_mode", "native"))
        return p
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_conversati_9e4fef: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
