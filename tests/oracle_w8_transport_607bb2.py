#!/usr/bin/env python3
"""Differential oracle for tui_gateway/transport.py (gate w8_transport_607bb2).

serialize_frame's happy path is replayed with the LIVE function (a no-op logger
suffices); TeeTransport.write is replayed with injected primary/secondary
transports so secondary exceptions exercise the same swallow path as C.
"""
import importlib.util
import json
import logging
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


live = load("w8_transport_607bb2_live", QUARRY + "/tui_gateway/transport.py")
log = logging.getLogger("w8_transport_607bb2")


class _T:
    def __init__(self, ok, do_raise):
        self.ok, self.do_raise = ok, do_raise

    def write(self, obj):
        if self.do_raise:
            raise RuntimeError("secondary failure")
        return self.ok

    def close(self):
        return None


def compute(r):
    func = r.get("func")
    if func == "serialize_frame":
        obj = r.get("obj")
        return live.serialize_frame(obj if obj is not None else {}, "stdio", log)
    if func == "tee_write":
        primary = _T(bool(r.get("primary_ok", False)), False)
        seconds = [_T(bool(s.get("ok", False)), bool(s.get("raise", False)))
                   for s in r.get("secondaries", [])]
        return live.TeeTransport(primary, *seconds).write({})
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
        print("ORACLE w8_transport_607bb2: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w8_transport_607bb2 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w8_transport_607bb2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
