#!/usr/bin/env python3
"""Differential oracle for gateway/status_pid_records.py (w13_statuspid_56d22b).

gateway.status helpers are seams: _read_pid_record yields the fixture record,
_pid_from_record/_start_times_agree/_pid_exists/_get_process_start_time are
driven by the fixture's live_pids and live_start_times. The two record-selection
decisions run on the live module.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_statuspid_56d22b"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def stub(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


gw = stub("gateway")
PID_RECORD = [None]
LIVE_PIDS = [set()]
LIVE_START = [{}]


def pid_from(rec):
    if not isinstance(rec, dict):
        return None
    v = rec.get("pid")
    if isinstance(v, bool):
        return None
    if isinstance(v, (int, float)):
        return v
    if isinstance(v, str):
        try:
            return int(v)
        except ValueError:
            return None
    return None


def pids_from(values):
    out = set()
    for v in values or []:
        if isinstance(v, bool):
            continue
        if isinstance(v, int):
            out.add(v)
        elif isinstance(v, float) and v.is_integer():
            out.add(int(v))
        elif isinstance(v, str):
            try:
                out.add(int(v))
            except ValueError:
                pass
    return out


status = stub(
    "gateway.status",
    _read_pid_record=lambda path: PID_RECORD[0],
    _pid_from_record=pid_from,
    _start_times_agree=lambda a, b: a == b,
    _pid_exists=lambda pid: pid in LIVE_PIDS[0],
    _get_process_start_time=lambda pid: LIVE_START[0].get(int(pid)),
)
gw.status = status  # `from gateway import status` resolves the attribute

SRC = QUARRY + "/gateway/status_pid_records.py"
spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    PID_RECORD[0] = r.get("pid_record")
    LIVE_PIDS[0] = pids_from(r.get("live_pids"))
    LIVE_START[0] = {int(k): v for k, v in (r.get("live_start_times") or {}).items()}
    lock = r.get("lock_record")
    func = r.get("func")
    if func == "previous":
        return live.names_a_previous_gateway(PID_RECORD[0], lock)
    if func == "strict":
        return list(live.strict_identity_records("gateway.pid", lock, bool(r.get("pid_exists"))))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
