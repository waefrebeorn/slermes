#!/usr/bin/env python3
"""Differential oracle for gateway/session_managed_worker.py (gate w18_sessionman_98782b).

Top-level imports are stubbed; the three ported functions are self-contained.
The store fixture is a JSON object; the oracle materialises a minimal store
whose ``sessions.get`` and ``lineage.is_compression_child`` mirror the C seam.
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


class AuthorityRefusal(Exception):
    pass


agent = _mod("agent")
agent.__path__ = []
_mod("agent.managed_worker",
     ACTIVITY_FRAME="", accept_result=lambda *a, **k: None,
     activity_interval=lambda x: x, encode_frame=lambda *a: b"",
     read_frame=lambda *a: None)
gateway = _mod("gateway")
gateway.__path__ = []
_mod("gateway.session_worker_reservation",
     owner_worker_subject=lambda *a: "", reserve_admission_worker=lambda *a, **k: None)
persistence = _mod("persistence")
persistence.__path__ = []
core = _mod("persistence.core")
core.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

SRC = Q + "/gateway/session_managed_worker.py"
spec = importlib.util.spec_from_file_location("w18_sessionman_98782b_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w18_sessionman_98782b_live"] = live
spec.loader.exec_module(live)


def _store(d):
    sessions = d.get("sessions", {}) if isinstance(d, dict) else {}

    class Row:
        def __init__(self, rd):
            self.parent_session_id = rd.get("parent_session_id")
            self._cc = bool(rd.get("compression_child", False))

    class Sessions:
        def get(self, k):
            v = sessions.get(k)
            return Row(v) if isinstance(v, dict) else None

    class Lineage:
        def is_compression_child(self, row):
            return bool(getattr(row, "_cc", False))

    s = types.SimpleNamespace()
    s.sessions = Sessions()
    s.lineage = Lineage()
    return s


def compute(r):
    func = r.get("func")
    if func == "continuation_path":
        return live._continuation_path(_store(r.get("store", {})),
                                       r.get("start", ""), r.get("tip", ""))
    if func == "compression_ancestors":
        return live._compression_ancestors(_store(r.get("store", {})),
                                           r.get("session_id", ""))
    if func == "with_stop_cause":
        worker = types.SimpleNamespace(stop_cause=r.get("worker", {}).get("stop_cause"))
        return live._with_stop_cause(r.get("accepted", {}), worker)
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_sessionman_98782b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
