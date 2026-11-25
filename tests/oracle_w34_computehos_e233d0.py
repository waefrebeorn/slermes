#!/usr/bin/env python3
"""Differential oracle for tui_gateway/compute_host_bridge.py (gate w34_computehos_e233d0).

The live module is imported as a package module and its value functions are
called with the same inputs the C port takes.  The bridge binds cross-module
helpers onto its own globals at install time (``method_ctx.bind_module``); the
oracle performs that same binding from the live sibling modules, so the
turn.start payload is shaped by the real ``_session_cwd`` /
``_context_cwd_is_launch_artifact`` / ``_session_source`` /
``_session_auth_user_id`` and the real ``_err`` envelope.

Seams kept out of the comparison (they are transport/config, never values):
the HostSupervisor child (a stub records the control frame it was handed), the
registry lease transfer on a rotated session_key, config.yaml reads and the
vault-redaction registry.
"""
import json
import os
import sys
import threading
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w34_computehos_e233d0"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

# The proof shell (`bash -lc`) clears USERPROFILE, and pathlib.Path.home() then
# raises inside the quarry's import chain.  Nothing under test reads the home
# directory, so restore one from the workspace the repo was found in.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

import tui_gateway.compute_host_bridge as live  # noqa: E402
from tui_gateway import server as _server  # noqa: E402
from tui_gateway.session_transports import _session_auth_user_id  # noqa: E402
from tui_gateway.session_workdir import (  # noqa: E402
    _context_cwd_is_launch_artifact,
    _session_cwd,
    _session_source,
)

WANTED = {
    "turn_isolation_enabled",
    "session_uses_compute_host",
    "active_session_lease_vouch",
    "metadata_mirror",
    "open_request_matches",
    "adopt_frame_meta",
    "control_frame",
    "compress_wait_seconds",
    "ack_error",
    "turn_frame",
}

# --- install-time bindings (what method_ctx.bind_module does in the live server) ---
live._session_cwd = _session_cwd
live._context_cwd_is_launch_artifact = _context_cwd_is_launch_artifact
live._session_source = _session_source
live._session_auth_user_id = _session_auth_user_id
live._err = _server._err
live._ok = _server._ok
live.os = _server.os  # bound module global: the bridge reads os.environ bare
live.logger = types.SimpleNamespace(
    debug=lambda *a, **k: None, warning=lambda *a, **k: None, info=lambda *a, **k: None)


class _Lease:
    """The registry lease as the bridge reads it (attribute access)."""

    def __init__(self, raw):
        self.lease_id = raw.get("lease_id")
        self.session_id = raw.get("session_id")
        self.released = bool(raw.get("released"))
        self.enabled = bool(raw.get("enabled"))


def _session(raw):
    """A live-shaped session record: real lock, lease object instead of a dict."""
    if not isinstance(raw, dict):
        return raw
    session = dict(raw)
    session["history_lock"] = threading.Lock()
    lease = session.get("active_session_lease")
    if isinstance(lease, dict):
        session["active_session_lease"] = _Lease(lease)
    return session


def _child_env(req):
    value = req.get("child_env_is_1")
    os.environ["HERMES_COMPUTE_HOST_CHILD"] = "1" if value else ""


class _FakeHost:
    """Supervisor seam: records the frame the bridge hands a control RPC."""

    def __init__(self):
        self.payload = None

    def control(self, sid, *, route_name, payload, wait=True, timeout=30.0, on_late_ack=None):
        self.payload = payload
        return {"type": "control.ok", "result": {"ok": True}}


_FAKE_HOST = _FakeHost()
live._get_compute_host_supervisor = lambda cfg=None: _FAKE_HOST

_ROTATIONS = {"n": 0}


def _transfer_active_session_slot(sid, session, new_session_id=None):
    """Registry seam: count the re-anchors a rotated session_key triggers."""
    _ROTATIONS["n"] += 1
    return True


live._transfer_active_session_slot = _transfer_active_session_slot


# --- per-function live computations ---------------------------------------------


def turn_isolation_enabled(req):
    _child_env(req)
    return bool(live._turn_isolation_enabled(req.get("cfg")))


def session_uses_compute_host(req):
    _child_env(req)
    return bool(live._session_uses_compute_host(req.get("session") or {}, req.get("cfg")))


def active_session_lease_vouch(req):
    return live._active_session_lease_vouch(_session(req.get("session")))


def metadata_mirror(req):
    return live._metadata_mirror(_session(req.get("session")))


def open_request_matches(req):
    return bool(live._open_request_matches(_session(req.get("session")), req.get("request_id")))


def adopt_frame_meta(req):
    session = _session(req.get("session") or {})
    before = _ROTATIONS["n"]
    live._compute_host_adopt_frame_meta(session, req.get("frame") or {})
    return {
        "session_key": str(session.get("session_key") or ""),
        "history_version": session.get("history_version"),
        "lease_rotated": _ROTATIONS["n"] != before,
    }


def control_frame(req):
    _FAKE_HOST.payload = None
    command = req.get("command")
    live._send_compute_host_control(
        str(req.get("sid") or "s1"), route_name="test",
        command=command if isinstance(command, str) else "",
        payload=req.get("payload"))
    return _FAKE_HOST.payload


def compress_wait_seconds(req):
    return live._compute_host_compress_wait_seconds(req.get("cfg"))


def ack_error(req):
    return live._compute_host_ack_error(req.get("rid"), req.get("ack") or {},
                                        int(req.get("code") or 0), str(req.get("default") or ""))


def turn_frame(req):
    images = req.get("image_paths")
    kwargs = {}
    if images is not None:
        kwargs["image_paths"] = images
    return live._compute_host_turn_frame(
        req.get("rid"), req.get("sid"), _session(req.get("session")), req.get("text"),
        queued_prompt_generation=req.get("queued_prompt_generation"),
        display_kind=req.get("display_kind"), display_metadata=req.get("display_metadata"),
        **kwargs)


HANDLERS = {
    "turn_isolation_enabled": turn_isolation_enabled,
    "session_uses_compute_host": session_uses_compute_host,
    "active_session_lease_vouch": active_session_lease_vouch,
    "metadata_mirror": metadata_mirror,
    "open_request_matches": open_request_matches,
    "adopt_frame_meta": adopt_frame_meta,
    "control_frame": control_frame,
    "compress_wait_seconds": compress_wait_seconds,
    "ack_error": ack_error,
    "turn_frame": turn_frame,
}


def expected(r):
    return HANDLERS[r["func"]](r)


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
    if cases == 0:
        print("ORACLE %s: FAILED GATE -- no cases ran" % GATE, file=sys.stderr)
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
