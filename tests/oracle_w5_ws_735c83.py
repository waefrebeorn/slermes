#!/usr/bin/env python3
"""Differential oracle for tui_gateway/ws.py (gate w5_ws_735c83).

Only pure framing/classification seams are exercised. tui_gateway's heavy
imports resolve from the quarry on sys.path; is_response_frame is injected so
_answers_open_request is deterministic.
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

SRC = Q + "/tui_gateway/ws.py"
spec = importlib.util.spec_from_file_location("ws_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["ws_live"] = live
spec.loader.exec_module(live)

srvreq = types.ModuleType("tui_gateway.server_requests")
srvreq.is_response_frame = lambda req: False
sys.modules["tui_gateway.server_requests"] = srvreq


class Obj:
    pass


def ws_for(client):
    ws = Obj()
    if client is not None:
        c = Obj()
        for key in ("host", "port"):
            if key in client:
                setattr(c, key, client[key])
        ws.client = c
    return ws


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "peer":
            py = live._ws_peer_label(ws_for(r.get("client")))
        elif func == "unknown":
            py = live._is_unknown_method(r.get("resp"))
        elif func == "concurrent":
            py = r.get("method", "") in live._WS_CONCURRENT_READ_METHODS
        elif func == "streaming":
            py = r.get("event", "") in live._STREAMING_EVENT_TYPES
        elif func == "answers":
            srvreq.is_response_frame = lambda req, v=bool(r.get("is_response", False)): v
            py = live._answers_open_request(r.get("req"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_ws_735c83: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
