#!/usr/bin/env python3
"""Differential oracle for cron/cancellation.py (noc_w6_cancellation_b52bf7.h)."""
import importlib.util
import json
import pathlib
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/cron/cancellation.py"
sys.path.insert(0, QUARRY)

if "cron" not in sys.modules:
    _pkg = types.ModuleType("cron")
    _pkg.__path__ = [QUARRY + "/cron"]
    sys.modules["cron"] = _pkg
_constants = types.ModuleType("hermes_constants")
_constants.get_hermes_home = lambda: pathlib.Path(QUARRY) / ".hermes"
sys.modules["hermes_constants"] = _constants
_exec = types.ModuleType("cron.executions")
_exec.cancellable_execution = lambda job_id: None
sys.modules["cron.executions"] = _exec

spec = importlib.util.spec_from_file_location("cancellation_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cancellation_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", round(float(x), 9))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


class _Event:
    def __init__(self, is_set, reason=None):
        self._set = bool(is_set)
        self.reason = reason

    def is_set(self):
        return self._set


def _events(specs):
    out = []
    for s in specs or []:
        out.append(_Event(s.get("set", False), s.get("reason")))
    return out


def compute(r):
    f = r["func"]
    if f == "valid":
        try:
            live._marker_path(r.get("id"))
            return True
        except (ValueError, TypeError):
            return False
    if f == "marker":
        try:
            return live._marker_path(r.get("id")).name
        except (ValueError, TypeError):
            return None
    if f == "cancel_reason":
        reason = r.get("reason")
        ev = types.SimpleNamespace(reason=reason) if reason is not None else object()
        return live.cancel_reason(ev)
    if f == "cancel_run_none":
        live.cancellable_execution = lambda job_id: None
        return live.cancel_run("job")
    if f == "cancel_run":
        rec = r.get("record")
        live.cancellable_execution = lambda job_id: rec
        live.request_cancel = lambda *a, **k: None
        return live.cancel_run("job", reason=r.get("reason"))
    if f == "combined_is_set":
        return live._CombinedCancelEvent(*_events(r.get("events"))).is_set()
    if f == "combined_reason":
        return live._CombinedCancelEvent(*_events(r.get("events"))).reason
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
        print(f"ORACLE cancellation: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    if _canon(py) != _canon(r.get("out")):
        mism += 1
        print(f"MISMATCH cancellation {r.get('func')} case={r.get('case')} "
              f"py={py!r} c={r.get('out')!r}", file=sys.stderr)
print(f"ORACLE cancellation: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
