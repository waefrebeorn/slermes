#!/usr/bin/env python3
"""Differential oracle for hermes_cli/stream_json.py (w8_streamjson_4c9aec gate)."""
import contextlib
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

LIVE_NAME = "w8_streamjson_4c9aec_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/stream_json.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "requested":
        a = r.get("args", {}) or {}
        args = types.SimpleNamespace(
            output_format=a.get("output_format", "text"),
            query=a.get("query"), query_file=a.get("query_file"),
            tui=a.get("tui", False), quiet=a.get("quiet", False))
        buf = io.StringIO()
        try:
            with contextlib.redirect_stderr(buf):
                ret = live.stream_json_requested(args)
            err = None
        except SystemExit:
            ret = None
            err = buf.getvalue().rstrip("\n")
        return {"requested": bool(ret), "error": err,
                "quiet": bool(getattr(args, "quiet", False))}
    if f == "text_delta":
        text = r.get("text")
        if not text:
            return None
        return {"type": "text", "text": str(text)}
    if f == "result_payload":
        dur = r.get("duration_ms", 0) or 0
        with contextlib.redirect_stdout(io.StringIO()):
            em = live.StreamJsonEmitter(model="", session_id="")
        em._session_id = r.get("bound_session_id", "")
        em._start = 0.0
        class FakeTime:
            def time(self):
                return dur / 1000.0
        live.time = FakeTime()
        captured = {}
        em._emit = lambda obj: captured.setdefault("payload", obj)
        with contextlib.redirect_stderr(io.StringIO()):
            em.emit_result(r.get("result"), session_id=r.get("session_id", ""),
                           exit_code=r.get("exit_code"))
        return captured["payload"]
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH streamjson %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_streamjson_4c9aec: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
