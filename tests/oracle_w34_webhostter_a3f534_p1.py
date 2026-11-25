#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_host_terminal_sessions.py, part 1
(gate w34_webhostter_a3f534_p1): registry shaping and admission decisions.

The live module is imported as a package module and driven on purpose-built
objects, so each compared value comes from the real code path:

  * ``_request_identity`` / ``_home_live`` / ``_request_home`` are called
    directly; the filesystem and profile-name resolution are seams replaced by
    ``FakePath`` and a two-function patch (``assert_named_profile_home_live``,
    ``web_server_profiles._resolve_profile_dir``);
  * ``HostTerminalRegistry.resolve`` runs for real on a registry made with
    ``__new__`` (no connection, no executor, no loop-bound state) whose
    sessions/owners/exits are fakes -- including a reaping ``dict`` that hides
    the token between the two ``_sessions.get`` calls;
  * ``get_host_terminals`` and ``host_terminal_lifespan`` run for real against
    a namespace app, so the teardown stack is observed, not re-modelled;
  * ``reap_idle`` runs one tick for real over fake sessions/owners.

Everything that is a process, a socket, a thread or a filesystem read stays a
seam: only what the values decide is compared.
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
GATE = "w34_webhostter_a3f534_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

HTTPException = _install_seams()

import hermes_cli.web_host_terminal_sessions as live  # noqa: E402

# Seam: profile-name resolution belongs to web_server_profiles (it validates,
# normalises and stats the profile directory).  That module is replaced by the
# fixture's profile table so the oracle compares _request_home's decisions, not
# profile storage -- and so importing it cannot need a process HOME.
_profiles = types.ModuleType("hermes_cli.web_server_profiles")
_profiles.__doc__ = "seam: _resolve_profile_dir comes from the fixture's table"
sys.modules["hermes_cli.web_server_profiles"] = _profiles

WANTED = {
    "constants", "request_identity", "home_live", "request_home",
    "resolve_verdict", "get_registry_state", "lifespan_exit", "reap_plan",
}


class FakePath:
    """A path whose filesystem answers are the fixture's: ``resolve()`` is the
    caller's job (a seam), equality is textual after it."""

    def __init__(self, text, live_=True, named=False, exists=True, tombstoned=False):
        self.text = str(text)
        self.live_ = bool(live_)
        self.named = bool(named)
        self.exists = bool(exists)
        self.tombstoned = bool(tombstoned)

    def resolve(self):
        return self

    def is_dir(self):
        return self.live_

    def __str__(self):
        return self.text

    def __repr__(self):
        return "FakePath(%r)" % (self.text,)

    def __eq__(self, other):
        return self.text == (other.text if isinstance(other, FakePath) else str(other))

    def __hash__(self):
        return hash(self.text)


def _assert_home_live(home):
    """hermes_constants.assert_named_profile_home_live over a FakePath."""
    if home.named and (home.tombstoned or not home.exists):
        raise FileNotFoundError(
            "Named profile home does not exist: %s. Create the profile explicitly "
            "before using it." % (home,))


live.assert_named_profile_home_live = _assert_home_live


def _install_request_home(req):
    """Patch the two seams ``_request_home`` reaches: profile-name resolution and
    the current home.  Both are filesystem/config lookups, not decisions."""
    profiles = req.get("profiles") or {}
    current = req.get("current_home") or ""

    def resolve(name):
        if name in profiles:
            return FakePath(profiles[name])
        raise HTTPException(status_code=404, detail="Profile '%s' does not exist." % name)

    _profiles._resolve_profile_dir = resolve
    live.get_hermes_home = lambda: FakePath(current)


class _Bridge:
    def __init__(self, alive):
        self._alive = bool(alive)

    def is_alive(self):
        return self._alive


class _Session:
    def __init__(self, alive=True, bridge_alive=True, attached=True, detached=None):
        self.alive = bool(alive)
        self.bridge = _Bridge(bridge_alive)
        self.attached = bool(attached)
        self.last_detached_at = detached
        self.closed = 0

    async def close(self):
        self.closed += 1


class _ReapingDict(dict):
    """``_sessions`` while the reaper takes the token during validation: the
    first lookup finds it, the identity re-check after ``to_thread`` does not."""

    def get(self, key, default=None):
        value = dict.get(self, key, default)
        if key in self:
            del self[key]
        return value


def _bare_registry():
    """A registry with no executor, no loop and no sessions: only the values."""
    r = live.HostTerminalRegistry.__new__(live.HostTerminalRegistry)
    r._ttl = live.RETENTION_SECONDS
    r._max = live._MAX_SHELLS
    r._buffer_cap = 1024 * 1024
    r._read_timeout = 0.2
    r._drain_executor = None
    r._sessions = {}
    r.owners = {}
    r.exits = {}
    r._closed = False
    r._background_closes = set()
    r._spawn_cleanup = set()
    return r


def constants(_req):
    from hermes_cli.pty_session import (
        WS_CLOSE_PROCESS_EXITED, WS_CLOSE_PROCESS_EXITED_REASON,
        WS_CLOSE_VIEWER_STALLED, WS_CLOSE_VIEWER_STALLED_REASON,
        _VIEWER_SEND_TIMEOUT_SECONDS,
    )
    from hermes_cli.web_server_chat import _RESIZE_RE

    return {
        "retention_seconds": live.RETENTION_SECONDS,
        "max_shells": live._MAX_SHELLS,
        "exits_kept": live.HostTerminalRegistry._EXITS_KEPT,
        "input_queue_frames": live._INPUT_QUEUE_FRAMES,
        "input_queue_bytes": live._INPUT_QUEUE_BYTES,
        "frame_overhead_bytes": live._FRAME_OVERHEAD_BYTES,
        "liveness_probe_seconds": live._LIVENESS_PROBE_SECONDS,
        "resize_answer_seconds": live._RESIZE_ANSWER_SECONDS,
        "viewer_send_timeout_seconds": _VIEWER_SEND_TIMEOUT_SECONDS,
        "ws_close_process_exited": WS_CLOSE_PROCESS_EXITED,
        "ws_close_process_exited_reason": WS_CLOSE_PROCESS_EXITED_REASON,
        "ws_close_viewer_stalled": WS_CLOSE_VIEWER_STALLED,
        "ws_close_viewer_stalled_reason": WS_CLOSE_VIEWER_STALLED_REASON,
        "token_regex": live.TOKEN_RE.pattern,
        "meta_prefix_bytes": list(live.META_PREFIX.encode("utf-8")),
        "resize_regex": _RESIZE_RE.pattern.decode("latin-1"),
    }


def request_identity(req):
    ws = types.SimpleNamespace(_hermes_auth_identity=req.get("identity"))
    try:
        provider, user = live._request_identity(ws)
    except live.TerminalDenied:
        return {"denied": True}
    return {"provider": provider, "user": user}


def home_live(req):
    home = FakePath(
        "C:/h/x",
        live_=bool(req.get("is_dir")),
        named=bool(req.get("named")),
        exists=bool(req.get("exists")),
        tombstoned=bool(req.get("tombstoned")),
    )
    return live._home_live(home)


def request_home(req):
    _install_request_home(req)
    try:
        return {"home": str(live._request_home(req.get("profile")))}
    except HTTPException:
        return {"expired": True}


def _owner(req):
    return live.HostOwner(
        tuple(req.get("owner_identity") or ("", "")),
        FakePath(req.get("owner_home") or "", live_=bool(req.get("home_live"))),
        "bash",
        "/",
    )


def resolve_verdict(req):
    r = _bare_registry()
    _install_request_home(req)
    token = req.get("token")
    session = _Session(
        alive=bool(req.get("alive")),
        bridge_alive=bool(req.get("bridge_alive")),
        attached=True,
    )
    if req.get("session_present"):
        r._sessions[token] = session
    if req.get("owner_present"):
        r.owners[token] = _owner(req)
    for tok, ident in (req.get("exits") or {}).items():
        r.exits[tok] = (tuple(ident), 0.0)
    if req.get("reap_during_validate"):
        r._sessions = _ReapingDict(r._sessions)
    r.expired = lambda s, now=None: bool(req.get("expired"))
    ws = types.SimpleNamespace(
        state=types.SimpleNamespace(
            ssh_terminal="ssh" if req.get("ssh_terminal") else None),
        query_params={"profile": req.get("request_profile")},
    )
    try:
        asyncio.run(r.resolve(token, ws, tuple(req.get("identity") or ("", "")),
                              closing=bool(req.get("closing"))))
    except live.TerminalExited:
        return {"verdict": "exited"}
    except live.TerminalExpired:
        return {"verdict": "expired"}
    except live.TerminalDenied:
        return {"verdict": "denied"}
    except HTTPException:
        return {"verdict": "expired"}
    return {"verdict": "ok"}


def get_registry_state(req):
    state = types.SimpleNamespace()
    if req.get("present"):
        state.host_terminals = types.SimpleNamespace(_closed=bool(req.get("closed")))
    app = types.SimpleNamespace(state=state)
    try:
        live.get_host_terminals(app)
    except RuntimeError as exc:
        return {"error": str(exc)}
    return {"ok": True}


def lifespan_exit(req):
    app = types.SimpleNamespace(state=types.SimpleNamespace())
    seeded = req.get("seeded")
    if isinstance(seeded, list):
        app.state._host_terminal_registries = list(seeded)
    host_now = req.get("host_now")

    async def run():
        async with live.host_terminal_lifespan(app):
            # teardown order: one lifespan can exit while another registry is
            # still the app's current one (a nested/overlapping lifespan).
            if host_now == "ME":
                pass
            elif host_now is None:
                if hasattr(app.state, "host_terminals"):
                    del app.state.host_terminals
            else:
                app.state.host_terminals = host_now

    asyncio.run(run())
    regs = getattr(app.state, "_host_terminal_registries", None)
    return {
        "registries": None if regs is None else list(regs),
        "host_terminals": getattr(app.state, "host_terminals", None),
        "has_registries": hasattr(app.state, "_host_terminal_registries"),
        "has_host_terminals": hasattr(app.state, "host_terminals"),
    }


def reap_plan(req):
    r = _bare_registry()
    for owner in req.get("owners") or []:
        r.owners[owner.get("token")] = live.HostOwner(
            tuple(owner.get("identity") or ("", "")),
            FakePath("C:/h/x", live_=bool(owner.get("live"))),
            "bash",
            "/",
        )
    for entry in req.get("sessions") or []:
        r._sessions[entry.get("token")] = _Session(
            alive=bool(entry.get("alive")),
            bridge_alive=bool(entry.get("bridge_alive")),
            attached=bool(entry.get("attached")),
            detached=entry.get("last_detached_at"),
        )
    for row in req.get("exits") or []:
        r.exits[row[0]] = (tuple(row[1]), float(row[2]))
    asyncio.run(r.reap_idle(now=float(req.get("now") or 0.0)))
    return {
        "sessions": list(r._sessions),
        "owners": list(r.owners),
        "exits": [[tok, list(ident), at] for tok, (ident, at) in r.exits.items()],
    }


HANDLERS = {
    "constants": constants,
    "request_identity": request_identity,
    "home_live": home_live,
    "request_home": request_home,
    "resolve_verdict": resolve_verdict,
    "get_registry_state": get_registry_state,
    "lifespan_exit": lifespan_exit,
    "reap_plan": reap_plan,
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
