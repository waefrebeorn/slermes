#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_host_terminal_sessions.py, part 2
(gate w34_webhostter_a3f534_p2): payload building and the input pump's value
decisions.

Every compared value comes from the real code: ``_metadata`` and
``_send_closed`` are called on namespace owners and a capture WebSocket,
``_frame_payload`` / ``_frame_cost`` / ``_input_runs`` / ``_resize`` /
``_admit`` / ``_InputQueue.full`` and TOKEN_RE are driven directly.  The seams
are the ones the module itself keeps out of its values: the socket (send/close),
the PTY session (size/resize/wait_for_output/outputs), the registry behind
``_admit`` and the clock.

Payloads are compared as byte arrays because META_PREFIX begins with NUL.
"""
import asyncio
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_webhostter_a3f534_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

import hermes_cli.web_host_terminal_sessions as live  # noqa: E402
from hermes_cli.web_server_chat import _RESIZE_RE  # noqa: E402

WANTED = {
    "metadata_payload", "closed_payload", "frame_payload", "frame_cost",
    "queue_full", "input_runs", "resize_undo", "admit_verdict", "token_ok",
}


def _frame(obj):
    """Fixture frame -> the dict a WebSocket receive() would hand the pump."""
    if not isinstance(obj, dict):
        return obj
    frame = dict(obj)
    raw = frame.get("bytes")
    if isinstance(raw, list):
        frame["bytes"] = bytes(raw)
    return frame


class _CaptureWS:
    """The socket seam: records exactly what the module sends and closes with."""

    def __init__(self, params=None, identity=None):
        self.query_params = params or {}
        self._hermes_auth_identity = identity
        self.texts = []
        self.codes = []
        self.reasons = []

    async def send_text(self, text):
        self.texts.append(text)

    async def close(self, code=None, reason=None):
        self.codes.append(code)
        self.reasons.append(reason)


def metadata_payload(req):
    owner = types.SimpleNamespace(shell=req.get("shell"), cwd=req.get("cwd"))
    registry = types.SimpleNamespace(_ttl=req.get("ttl", live.RETENTION_SECONDS))
    text = live._metadata(
        req.get("token"), owner, registry,
        reconnected=bool(req.get("reconnected")),
        truncated=bool(req.get("truncated")),
    )
    return {"bytes": list(text.encode("utf-8"))}


def closed_payload(req):
    ws = _CaptureWS()
    asyncio.run(live._send_closed(ws, req.get("token")))
    return {
        "bytes": list(ws.texts[0].encode("utf-8")) if ws.texts else [],
        "codes": list(ws.codes),
    }


def frame_payload(req):
    return {"bytes": list(live._frame_payload(_frame(req.get("msg"))))}


def frame_cost(req):
    return live._frame_cost(_frame(req.get("item")))


def queue_full(req):
    inbox = live._InputQueue()
    for _ in range(int(req.get("frames") or 0)):
        inbox.put_nowait({"text": "x"})
    # ``queued`` is the charge _put/_get maintain; the fixture sets the total so
    # the comparison is about the bound, not about filling a megabyte.
    inbox.queued = int(req.get("queued") or 0)
    return inbox.full()


def input_runs(req):
    frames = [_frame(f) for f in (req.get("frames") or [])]
    out = []
    for item in live._input_runs(frames, _RESIZE_RE):
        if isinstance(item, tuple):
            out.append({"kind": "resize", "cols": item[0], "rows": item[1]})
        else:
            out.append({"kind": "bytes", "data": list(item)})
    return out


class _ResizeSession:
    """The PTY session seam: records the wait and the resize it was given."""

    def __init__(self, before, outputs):
        self._before = None if before is None else tuple(before)
        self.outputs = outputs
        self.waited = None
        self.resized = None

    async def wait_for_output(self, mark, timeout):
        self.waited = mark

    def size(self):
        return self._before

    def resize(self, ws, cols, rows):
        self.resized = (cols, rows)


def resize_undo(req):
    undo_in = req.get("undo")
    undo = (tuple(undo_in[0]), undo_in[1]) if undo_in else None
    session = _ResizeSession(req.get("before"), req.get("outputs"))
    size = tuple(req.get("size") or (0, 0))
    nxt = asyncio.run(live._resize(session, None, size, undo))
    return {
        "waited_for": session.waited,
        "resize": list(session.resized) if session.resized else None,
        "undo": [list(nxt[0]), nxt[1]] if nxt is not None else None,
    }


class _StubRegistry:
    """The registry seam: create/resolve/remove record instead of spawning."""

    def __init__(self, sessions):
        self._sessions = {tok: object() for tok in sessions}
        self.removed = []

    async def create(self, ws, identity):
        return ("created", None)

    async def resolve(self, token, ws, identity, *, closing=False):
        return ("resolved", None)

    async def remove(self, token):
        self.removed.append(token)


def admit_verdict(req):
    registry = _StubRegistry(req.get("sessions") or [])
    ws = _CaptureWS(
        params={"attach": req.get("attach"), "action": req.get("action")},
        identity=req.get("identity"),
    )
    try:
        admitted = asyncio.run(live._admit(ws, registry))
    except live.TerminalDenied:
        return {"result": "denied", "removed": [], "sent": [], "codes": []}
    except live.TerminalExpired:
        return {"result": "expired", "removed": [], "sent": [], "codes": []}
    return {
        "result": "none" if admitted is None else admitted[0],
        "removed": list(registry.removed),
        "sent": [list(t.encode("utf-8")) for t in ws.texts],
        "codes": list(ws.codes),
    }


def token_ok(req):
    return bool(live.TOKEN_RE.fullmatch(req.get("token")))


HANDLERS = {
    "metadata_payload": metadata_payload,
    "closed_payload": closed_payload,
    "frame_payload": frame_payload,
    "frame_cost": frame_cost,
    "queue_full": queue_full,
    "input_runs": input_runs,
    "resize_undo": resize_undo,
    "admit_verdict": admit_verdict,
    "token_ok": token_ok,
}


def same(py, got):
    """Equality that keeps bools and numbers apart (Python's == does not)."""
    if isinstance(py, bool) or isinstance(got, bool):
        return isinstance(py, bool) and isinstance(got, bool) and py == got
    if isinstance(py, dict) or isinstance(got, dict):
        if not (isinstance(py, dict) and isinstance(got, dict)):
            return False
        if set(py) != set(got):
            return False
        return all(same(py[k], got[k]) for k in py)
    if isinstance(py, list) or isinstance(got, list):
        if not (isinstance(py, list) and isinstance(got, list)):
            return False
        return len(py) == len(got) and all(same(a, b) for a, b in zip(py, got))
    if isinstance(py, (int, float)) and isinstance(got, (int, float)):
        return py == got
    if isinstance(py, str) and isinstance(got, str):
        return py == got
    return py == got


def expected(req):
    func = req.get("func")
    handler = HANDLERS.get(func)
    if handler is None:
        raise AssertionError("unknown func %r" % (func,))
    return handler(req)


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
        if not same(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
