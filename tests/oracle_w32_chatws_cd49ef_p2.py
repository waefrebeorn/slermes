#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/chat_ws.py (gate p2).

Same file-path load and seam stubs as gate p1, but this half fixes the
sidecar surfaces:

  * ``_close_unless_sidecar_allowed`` -- chat-enabled / auth / request gate
    -> accept, or the close code (4403 / 4401) it sends;
  * ``_get_event_state`` -- the lazily created (event_channels, event_lock);
  * ``_lease_holder_pid`` -- the live single-writer lease holder for a session;
  * ``_broadcast_event`` -- the fan-out of one publisher frame to subscribers;
  * ``_pty_channel_marker_state`` -- (marker file, preexisting, resume).

The websocket, the lease registry, the app's event map and the marker file are
the seams: every one is supplied by the fixture, and only the decision taken
over them is compared.
"""
import asyncio
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
GATE = "w32_chatws_cd49ef_p2"

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location(
    "chat_ws_w32_p2", QUARRY / "hermes_cli" / "web_routers" / "chat_ws.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["chat_ws_w32_p2"] = live
_spec.loader.exec_module(live)

WANTED = {"sidecar_gate", "event_state", "lease_holder", "broadcast", "marker_state"}

# The failing-subscriber cases exercise a logged warning; keep the gate's
# stderr for mismatches only.
import logging  # noqa: E402

live._log = logging.getLogger("chat_ws_w32_p2")
live._log.addHandler(logging.NullHandler())
live._log.propagate = False

SENTINEL_LOCK = "event-lock"


class FakeWS:
    def __init__(self, query=None, name="ws"):
        self.query_params = dict(query or {})
        self.name = name
        self.closed = []
        self.got = []
        self.fail = False
        self.app = types.SimpleNamespace(state=types.SimpleNamespace())

    async def close(self, code=1000, reason=None):
        self.closed.append(code)

    async def send_text(self, payload):
        if self.fail:
            raise RuntimeError("subscriber went away")
        self.got.append(payload)


def sidecar_gate(r):
    live._DASHBOARD_EMBEDDED_CHAT_ENABLED = bool(r.get("chat_enabled"))
    auth = bool(r.get("auth_ok"))
    allowed = bool(r.get("request_allowed"))

    def _auth(ws, allow_internal=False, socket=None):
        # _ws_auth_ok is awaited by the handler's caller, but the router calls
        # it synchronously inside _close_unless_sidecar_allowed.
        return auth

    live._ws_auth_ok = _auth
    live._ws_request_is_allowed = lambda ws: allowed
    ws = FakeWS()
    ok = asyncio.run(live._close_unless_sidecar_allowed(
        ws, allow_internal=bool(r.get("allow_internal")), socket=r.get("socket")))
    return {"accepted": ok, "closed": ws.closed}


def event_state(r):
    app = types.SimpleNamespace(state=types.SimpleNamespace())
    channels = r.get("channels") or {}
    if r.get("has_channels"):
        app.state.event_channels = {k: set(v) for k, v in channels.items()}
    if r.get("has_lock"):
        app.state.event_lock = SENTINEL_LOCK
    chans, lock = live._get_event_state(app)
    return {"created": lock is not SENTINEL_LOCK,
            "channels": {k: len(v) for k, v in chans.items()}}


def lease_holder(r):
    entries = r.get("entries") or []
    raises = bool(r.get("raises"))

    def _snapshot(registry_home=None):
        if raises:
            raise OSError("registry unreadable")
        return entries

    live.active_session_registry_snapshot = _snapshot
    return live._lease_holder_pid(r.get("session_id"), registry_home="/tmp/registry")


def broadcast(r):
    failing = set(r.get("failing") or [])
    subs = {}
    for ch, names in (r.get("channels") or {}).items():
        subs[ch] = [FakeWS(name=n) for n in names]
    for group in subs.values():
        for sub in group:
            sub.fail = sub.name in failing
    app = types.SimpleNamespace(state=types.SimpleNamespace())
    app.state.event_channels = subs
    app.state.event_lock = asyncio.Lock()
    payload = r.get("payload")
    asyncio.run(live._broadcast_event(app, r.get("channel"), payload))
    return [[s.name, p] for group in subs.values() for s in group for p in s.got]


def marker_state(r):
    tmp = Path(tempfile.mkdtemp())
    markers = list(r.get("marker_channels") or [])
    files = {ch: tmp / (ch + ".keep") for ch in markers}
    fake_server = types.ModuleType("hermes_cli.web_server")
    fake_server._get_pty_active_session_files = lambda app: files
    saved = sys.modules.get("hermes_cli.web_server")
    sys.modules["hermes_cli.web_server"] = fake_server
    try:
        active_name = r.get("active_file")
        path = None
        if active_name is not None:
            path = tmp / active_name
            doc = r.get("marker_doc")
            if doc is not None:
                path.write_text(doc, encoding="utf-8")
        live._active_session_file_for_channel = lambda app, channel: path
        query = {}
        if r.get("channel") is not None:
            query["channel"] = r["channel"]
        ws = FakeWS(query)
        found, preexisting, resume = asyncio.run(
            live._pty_channel_marker_state(ws, r.get("resume"), bool(r.get("force_fresh"))))
    finally:
        if saved is not None:
            sys.modules["hermes_cli.web_server"] = saved
        else:
            sys.modules.pop("hermes_cli.web_server", None)
    return {"file": found.name if found is not None else None,
            "preexisting": preexisting,
            "resume": resume}


def expected(r):
    f = r.get("func")
    if f == "sidecar_gate":
        return sidecar_gate(r)
    if f == "event_state":
        return event_state(r)
    if f == "lease_holder":
        return lease_holder(r)
    if f == "broadcast":
        return broadcast(r)
    if f == "marker_state":
        return marker_state(r)
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
