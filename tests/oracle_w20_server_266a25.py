#!/usr/bin/env python3
"""Differential oracle for plugins/google_meet/node/server.py (w20_server_266a25).

The live module is imported from the quarry.  `protocol.validate_request` is
stubbed to accept (auth is the protocol module's contract, not this one's), the
process_manager it lazily imports is replaced by a recording double, `time` is
pinned for the pong stamp, and token persistence is captured in memory.
"""
import asyncio
import importlib.util
import json
import os
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_server_266a25_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "google_meet", "node", "server.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

CALLS = []


def _pm_fn(name):
    def fn(*a, **k):
        CALLS.append(name)
        return {"seen": name}
    return fn


pm_double = types.ModuleType("plugins.google_meet.process_manager")
pm_double.start = _pm_fn("start_bot")
pm_double.stop = _pm_fn("stop")
pm_double.status = _pm_fn("status")
pm_double.transcript = _pm_fn("transcript")
pm_double._read_active = lambda: {}
sys.modules["plugins.google_meet.process_manager"] = pm_double

live._proto.validate_request = lambda msg, token: (True, None)

TMP = tempfile.mkdtemp(prefix="meetnode_")
FILE_INSTEAD_OF_DIR = os.path.join(TMP, "not-a-dir")
with open(FILE_INSTEAD_OF_DIR, "w", encoding="utf-8") as fh:
    fh.write("x")


def server(display_name="node-x"):
    srv = live.NodeServer(display_name=display_name, token_path=Path(TMP) / "tok.json")
    srv._token = "tok"
    return srv


def handle(msg, display_name="node-x"):
    return asyncio.run(server(display_name)._handle_request(msg))


def expected(r):
    f = r.get("func")
    if f == "start_bot":
        payload = r.get("payload") if isinstance(r.get("payload"), dict) else {}
        kwargs = {k: payload[k] for k in live._START_BOT_KEYS if k in payload}
        result = r.get("start_result")
        try:
            got = live._rpc_start_bot(payload, types.SimpleNamespace(
                start=lambda **kw: (CALLS.append(dict(kw)), result)[1]))
            return {"ok": True, "error": None, "kwargs": kwargs, "payload": got}
        except live._RpcError as exc:
            return {"ok": False, "error": str(exc), "kwargs": kwargs, "payload": None}
    if f == "say":
        active = r.get("active")
        if isinstance(active, dict) and isinstance(active.get("out_dir"), str):
            active = dict(active)
            mode = active["out_dir"]
            if mode == "__DIR__":
                active["out_dir"] = tempfile.mkdtemp(prefix="sayq_", dir=TMP)
            elif mode == "__FILE__":
                active["out_dir"] = FILE_INSTEAD_OF_DIR
        pm = types.SimpleNamespace(_read_active=lambda: active)
        return live._rpc_say(r.get("payload") or {}, pm)
    if f == "dispatch":
        t = r.get("type")
        payload = {"url": "https://meet.example/x", "text": "hi", "reason": "why"}
        resp = handle({"type": t, "id": "i-1", "payload": payload})
        if resp.get("type") == "pong":
            return {"kind": "ping"}
        if resp.get("type") == "error":
            return {"kind": "error", "message": resp["error"]}
        return {"kind": "rpc", "handler": t}
    if f == "pong":
        ts = r.get("ts", 0.0)
        live.time = types.SimpleNamespace(time=lambda: ts)
        msg = {"type": "ping", "id": r.get("id"), "payload": {}}
        return handle(msg, r.get("display_name", "node-x"))
    if f == "error_envelope":
        return live._proto.make_error(r.get("id"), r.get("reason"))
    if f == "ensure_token":
        written = []
        live.read_json = lambda p: r.get("token_data")
        live.atomic_json_write = lambda p, obj, mode=None: written.append(obj)
        srv = live.NodeServer(token_path=Path(TMP) / "tok2.json")
        srv._token = r.get("cached")
        tok = srv.ensure_token()
        generated = bool(written)
        return {"token": None if generated else tok, "generate": generated}
    if f == "start_bot_keys":
        return list(live._START_BOT_KEYS)
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
print("ORACLE w20_server_266a25: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
