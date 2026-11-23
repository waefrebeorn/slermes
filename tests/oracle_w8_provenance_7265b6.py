#!/usr/bin/env python3
"""Differential oracle for acp_adapter/provenance.py (gate w8_provenance_7265b6).

The persistence interface is injected as a JSON dict of session rows through a
tiny fake with .sessions.get(); both LIVE functions are then replayed verbatim.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w8_provenance_7265b6_live", QUARRY + "/acp_adapter/provenance.py")


class _Row:
    def __init__(self, d):
        self.parent_session_id = d.get("parent_session_id")
        self.end_reason = d.get("end_reason")


class _Sessions:
    def __init__(self, data):
        self._d = data

    def get(self, sid):
        v = self._d.get(sid)
        return _Row(v) if isinstance(v, dict) else None


class _Persistence:
    pass


def compute(r):
    sessions = r.get("sessions") or {}
    p = _Persistence()
    p.sessions = _Sessions(sessions)
    acp = r.get("acp_id", "")
    cur = r.get("current", "")
    prev = r.get("previous")
    func = r.get("func")
    if func == "build":
        return live.build_session_provenance(p, acp, cur, previous_hermes_session_id=prev)
    if func == "meta":
        return live.session_provenance_meta(p, acp, cur, previous_hermes_session_id=prev)
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
        print("ORACLE w8_provenance_7265b6: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w8_provenance_7265b6 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w8_provenance_7265b6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
