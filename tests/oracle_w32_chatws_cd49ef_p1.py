#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/chat_ws.py (gate p1).

The live module is loaded by file path.  Every seam it reaches is faked: the
websocket is a message list plus a frame sink, ``_ws_gate`` waves the peer
through, the console executor answers from the fixture's canned results, and
the console engine is never constructed.  What is compared is exactly what
those values decide:

  * ``console_ws`` -- the frame sequence a client frame list produces, plus the
    close codes it earns (frame shaping, frame routing, payload errors);
  * ``_console_json_payload`` -- text/bytes -> (payload, error);
  * ``_channel_or_close_code`` -- the channel/topic key a query string may name;
  * ``_read_active_session_file`` -- the session id a marker file yields.

The console worker is canned, so each started command's frames are emitted
before the next client frame is handled -- which is what the port models.  The
socket, the executor and the engine are the seams; nothing here awaits a real
PTY, thread or timer.
"""
import asyncio
import concurrent.futures
import importlib.util
import json
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_chatws_cd49ef_p1"

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "chat_ws_w32_p1", QUARRY / "hermes_cli" / "web_routers" / "chat_ws.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["chat_ws_w32_p1"] = live
_spec.loader.exec_module(live)

WANTED = {"console_run", "console_payload", "channel_key", "marker_session"}

# The console engine is never built: its constructor is a seam.
_engine_mod = types.ModuleType("hermes_cli.console_engine")
_engine_mod.HermesConsoleEngine = lambda *a, **k: types.SimpleNamespace(
    execute=lambda *aa, **kk: None)
sys.modules.setdefault("hermes_cli.console_engine", _engine_mod)

RESULT_DEFAULTS = {"status": "ok", "output": None, "command": None,
                   "confirmation_message": None}


class FakeWS:
    """A websocket as a frame sink + a queued receive() list."""

    def __init__(self, query=None, msgs=()):
        self.query_params = dict(query or {})
        self.msgs = list(msgs)
        self.sent = []
        self.closed = []
        self.app = types.SimpleNamespace(state=types.SimpleNamespace())

    async def accept(self):
        return None

    async def close(self, code=1000, reason=None):
        self.closed.append([code, reason])

    async def send_json(self, payload):
        self.sent.append(payload)

    async def send_text(self, payload):
        self.sent.append(payload)

    async def receive(self):
        if not self.msgs:
            raise RuntimeError("socket closed")
        # Yield between client frames so the canned worker settles first.
        await asyncio.sleep(0.02)
        return self.msgs.pop(0)


class FakeExecutor:
    """One canned result per submitted command, already resolved."""

    def __init__(self, results):
        self.results = list(results)

    def submit(self, fn, *a, **kw):
        fut = concurrent.futures.Future()
        res = self.results.pop(0) if self.results else types.SimpleNamespace(**RESULT_DEFAULTS)
        fut.set_result(res)
        return fut


async def _fake_gate(ws, kind):
    return ("peer", "mode", "cred")


def _noop_profile_dir(name):
    return None


def _msg_list(raw):
    out = []
    for m in raw if isinstance(raw, list) else []:
        if not isinstance(m, dict):
            continue
        msg = {"type": m.get("type")}
        if m.get("text") is not None:
            msg["text"] = m["text"]
        elif m.get("bytes_hex") is not None:
            msg["bytes"] = bytes.fromhex(m["bytes_hex"])
        out.append(msg)
    return out


def console_run(r):
    results = []
    for entry in r.get("results") or []:
        base = dict(RESULT_DEFAULTS)
        if isinstance(entry, dict):
            base.update(entry)
        results.append(types.SimpleNamespace(**base))

    live._ws_gate = _fake_gate
    live._resolve_profile_dir = _noop_profile_dir
    executor = FakeExecutor(results)          # one worker pool for the session
    live._get_console_executor = lambda: executor

    ws = FakeWS(r.get("query") or {}, _msg_list(r.get("msgs")))
    asyncio.run(live.console_ws(ws))
    return {"sent": ws.sent, "closed": ws.closed}


def console_payload(r):
    msg = {}
    if r.get("text") is not None:
        msg["text"] = r["text"]
    elif r.get("bytes_hex") is not None:
        msg["bytes"] = bytes.fromhex(r["bytes_hex"])
    payload, error = live._console_json_payload(msg)
    return {"payload": payload, "error": error}


def channel_key(r):
    value = r.get("channel")
    query = {"channel": value} if value is not None else {}
    ws = types.SimpleNamespace(query_params=query)
    return live._channel_or_close_code(ws)


def marker_session(r):
    tmp = Path(tempfile.mkdtemp())
    path = tmp / "active.json"
    doc = r.get("doc")
    if doc is None:
        return live._read_active_session_file(path)   # missing file: the seam
    path.write_text(doc, encoding="utf-8")
    return live._read_active_session_file(path)


def expected(r):
    f = r.get("func")
    if f == "console_run":
        return console_run(r)
    if f == "console_payload":
        return console_payload(r)
    if f == "channel_key":
        return channel_key(r)
    if f == "marker_session":
        return marker_session(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
