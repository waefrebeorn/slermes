#!/usr/bin/env python3
"""Deterministic case generator for monitoring_events_flow3 (>=5 cases/func).

Each case is {"func": name, "in": {...dataclass fields..., "io": {seam: script}}}.
The harness replays it through the C port and the oracle replays the SAME
scripts through the LIVE agent/monitoring/events.py dataclasses; their
{ret, trace, state} must agree, including wire (key) order.

    python tests/gen_monitoring_events_flow3_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "monitoring_events_flow3.jsonl"


def c(func, inp, io=None):
    rec = dict(inp)
    rec["io"] = io or {}
    return {"func": func, "in": rec}


def raises(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def seq(*items):
    return {"$seq": list(items)}


CASES = []

# ── GatewayHealthEvent (17 fields; EVENT=gateway_health) ──────────────────
CASES += [
    # 1. full field set, ts_ns provided (no seam, empty trace)
    c("GatewayHealthEvent", {
        "name": "gw-up", "gateway_state": "running", "old_state": "idle",
        "new_state": "running", "exit_reason": "restart", "restart_requested": True,
        "active_agents": 4, "gateway_busy": True, "gateway_drainable": False,
        "platform_count": 3, "fatal_platform_count": 1, "profile": "default",
        "install_id": "inst-1", "version": "1.2.3", "supervision_mode": "service",
        "pid": 4242, "ts_ns": 1001}),
    # 2. minimal -> all defaults, ts_ns via seam
    c("GatewayHealthEvent", {"name": "gw-min"}, {"time.time_ns": 2002}),
    # 3. partial fields, ts_ns provided
    c("GatewayHealthEvent", {"name": "gw-part", "gateway_state": "draining",
                             "active_agents": 9, "gateway_busy": False,
                             "pid": 77, "ts_ns": 3003}),
    # 4. ts_ns via $seq seam
    c("GatewayHealthEvent", {"name": "gw-seq"},
      {"time.time_ns": seq(4004)}),
    # 5. missing required -> TypeError
    c("GatewayHealthEvent", {"gateway_state": "running"}),
    # 6. seam raises alias IOError -> canonical OSError
    c("GatewayHealthEvent", {"name": "gw-raise-alias"},
      {"time.time_ns": raises("IOError")}),
    # 7. seam raises BaseException trio -> SystemExit passes through
    c("GatewayHealthEvent", {"name": "gw-raise-base"},
      {"time.time_ns": raises("SystemExit")}),
    # 8. seam raises py3.14 addition PythonFinalizationError
    c("GatewayHealthEvent", {"name": "gw-raise-pfe"},
      {"time.time_ns": raises("PythonFinalizationError")}),
]

# ── GatewayDiagnosticEvent (12 fields; EVENT=gateway_diagnostic) ───────────
CASES += [
    # 1. full field set, ts_ns provided
    c("GatewayDiagnosticEvent", {
        "name": "diag", "subsystem": "transport", "error_class": "ValueError",
        "error_code": "E42", "platform": "win32", "old_state": "up",
        "new_state": "down", "profile": "p1", "version": "2.0",
        "severity": "error", "ts_ns": 5005, "source_logger": "a.b"}),
    # 2. minimal -> defaults error_class/severity, ts_ns via seam
    c("GatewayDiagnosticEvent", {"name": "diag-min", "subsystem": "core"},
      {"time.time_ns": 6006}),
    # 3. partial, ts_ns provided
    c("GatewayDiagnosticEvent", {"name": "diag-part", "subsystem": "metrics",
                                 "error_class": "OSError", "severity": "critical",
                                 "ts_ns": 7007}),
    # 4. ts_ns via $seq seam
    c("GatewayDiagnosticEvent", {"name": "diag-seq", "subsystem": "sched"},
      {"time.time_ns": seq(8008)}),
    # 5. missing subsystem -> TypeError
    c("GatewayDiagnosticEvent", {"name": "diag-nosub"}),
    # 6. seam raises EOFError -> Exception subclass
    c("GatewayDiagnosticEvent", {"name": "diag-eof", "subsystem": "io"},
      {"time.time_ns": raises("EOFError")}),
    # 7. seam raises ValueError
    c("GatewayDiagnosticEvent", {"name": "diag-val", "subsystem": "io"},
      {"time.time_ns": raises("ValueError")}),
    # 8. seam raises KeyboardInterrupt (base trio)
    c("GatewayDiagnosticEvent", {"name": "diag-kb", "subsystem": "io"},
      {"time.time_ns": raises("KeyboardInterrupt")}),
]

# ── CronExecutionEvent (7 fields; EVENT=cron_execution) ───────────────────
CASES += [
    # 1. full field set, ts_ns provided
    c("CronExecutionEvent", {
        "status": "ok", "job_key": "cron:1", "source": "scheduler",
        "duration_ms": 1500, "delivery_outcome": "delivered",
        "error_class": "none", "ts_ns": 9009}),
    # 2. minimal -> default source="unknown", ts_ns via seam
    c("CronExecutionEvent", {"status": "running", "job_key": "cron:2"},
      {"time.time_ns": 10010}),
    # 3. partial, ts_ns provided
    c("CronExecutionEvent", {"status": "failed", "job_key": "cron:3",
                             "duration_ms": 0, "error_class": "TypeError",
                             "ts_ns": 11011}),
    # 4. ts_ns via $seq seam
    c("CronExecutionEvent", {"status": "ok", "job_key": "cron:4"},
      {"time.time_ns": seq(12012)}),
    # 5. missing job_key -> TypeError
    c("CronExecutionEvent", {"status": "ok"}),
    # 6. seam raises _IncompleteInputError (py3.14 addition)
    c("CronExecutionEvent", {"status": "ok", "job_key": "cron:6"},
      {"time.time_ns": raises("_IncompleteInputError")}),
    # 7. seam raises OSError
    c("CronExecutionEvent", {"status": "ok", "job_key": "cron:7"},
      {"time.time_ns": raises("OSError")}),
    # 8. seam raises GeneratorExit (base trio)
    c("CronExecutionEvent", {"status": "ok", "job_key": "cron:8"},
      {"time.time_ns": raises("GeneratorExit")}),
]

# ── to_dict (direct {"event": EVENT, **fields} serialization) ─────────────
CASES += [
    c("to_dict", {"event": "gateway_health",
                  "fields": {"name": "gw", "active_agents": 2, "ts_ns": 13013}}),
    c("to_dict", {"event": "gateway_diagnostic",
                  "fields": {"name": "d", "subsystem": "s", "severity": "warning",
                             "error_class": "unknown"}}),
    c("to_dict", {"event": "cron_execution",
                  "fields": {"status": "ok", "job_key": "c", "duration_ms": 8}}),
    c("to_dict", {"event": "custom",
                  "fields": {"a": 1, "b": [1, 2, 3], "c": {"x": True, "y": None}}}),
    c("to_dict", {"event": "empty", "fields": {}}),
    c("to_dict", {"event": "unicode-\u00e9",
                  "fields": {"msg": "caf\u00e9 \u2603", "n": 0}}),
]


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as fh:
        for rec in CASES:
            fh.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print(f"WROTE {OUT} ({len(CASES)} cases): {counts}")
    assert all(v >= 5 for v in counts.values()), "each func needs >=5 cases"


if __name__ == "__main__":
    main()
