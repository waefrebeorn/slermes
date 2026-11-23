#!/usr/bin/env python3
"""Differential oracle for tui_gateway/stream_segments.py (gate w15_streamseg_daa21d)."""
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


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("tui_gateway", QUARRY + "/tui_gateway")
spec = importlib.util.spec_from_file_location(
    "tui_gateway.stream_segments_live", QUARRY + "/tui_gateway/stream_segments.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.stream_segments_live"] = live
spec.loader.exec_module(live)


def _seg(d):
    s = live.TurnSegments()
    s.turn_id = d.get("turn_id", "tid")
    s.segment = int(d.get("segment", 0))
    s.offset = int(d.get("offset", 0))
    return s


def compute(r):
    f = r.get("func")
    if f == "utf16_len":
        return live._utf16_len(r.get("text", ""))
    if f == "start":
        return _seg(r).start()
    if f == "delta":
        return _seg(r).delta(r.get("text"))
    if f == "delta_advance":
        s = _seg(r)
        s.delta(r.get("text"))
        return s.offset
    if f == "seal":
        return _seg(r).seal()
    if f == "stamp_reply":
        seg = _seg(r) if r.get("has_segments") else None
        return live.stamp_reply_frame(seg, r.get("event", ""), r.get("payload"))
    if f == "stamp_session":
        sess = json.loads(json.dumps(r.get("session") or {}))
        cursor = sess.get(live.SESSION_KEY)
        if isinstance(cursor, dict):
            sess[live.SESSION_KEY] = _seg(cursor)
        return live.stamp_session_frame(sess, r.get("event", ""), r.get("payload"))
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
        print("PYERROR %r %r" % (r, exc), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE w15_streamseg_daa21d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
