#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/irc/adapter.py, identity and
admission (gate w33_adapter_eaa5eb_p2).

The live adapter is imported BY FILE PATH (its package __init__ drags the whole
gateway stack); host-blocked native extensions are stubbed first.  Seams are
replaced by recorders fed the same fixture values the C harness reads:

  * ``_handle_privmsg`` admission -> ``_dispatch_message`` recorded (own echo,
    CTCP ACTION folding, channel addressing, allowlist),
  * ``_handle_line`` 433 -> ``_send_raw`` recorded (nick collision ladder),
  * ``_sa_register`` -> a scripted ``pump`` feeding 432/433 then 001
    (standalone nick + retry math),
  * ``_standalone_send`` -> fake stream class, recording ``raw``, a stub
    ``_sa_register``, a no-op clock and the send_error envelope
    (target guard, payload plan),
  * ``_env_enablement`` / ``_server_channel`` / ``validate_config`` -> the
    secret store replaced by the fixture's env map.

No socket opens, no thread starts.
"""
import asyncio
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_adapter_eaa5eb_p2"

# The package import chain reaches hermes_constants, which needs a home.
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
sys.path.append(str(Path(__file__).resolve().parent))

from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location(
    "irc_adapter_live_p2", str(QUARRY / "plugins" / "platforms" / "irc" / "adapter.py"))
live = importlib.util.module_from_spec(_spec)
sys.modules["irc_adapter_live_p2"] = live
_spec.loader.exec_module(live)

import gateway.platforms._shared as shared  # noqa: E402

ADAPTER = live.IRCAdapter
SA_PREFIX = "IRC standalone send: "

WANTED = {
    "privmsg_decision", "next_nick_collision", "standalone_nick_base",
    "standalone_nick", "nick_retry", "standalone_target", "standalone_lines",
    "is_channel", "chat_info", "env_enablement", "server_channel", "config_ready",
}

DROPPED = {"dispatch": False, "text": None, "chat_id": None, "chat_type": None,
           "user_id": None, "user_name": None}


def bare():
    return ADAPTER.__new__(ADAPTER)


# ── stream doubles ──────────────────────────────────────────────────────────

class _Reader:
    async def read(self, n):
        return b""


class _Writer:
    pass


class _Async:
    """asyncio with every blocking operation replaced by the fixture."""

    CancelledError = asyncio.CancelledError
    TimeoutError = TimeoutError

    @staticmethod
    def wait_for(aw, timeout=None):
        return aw

    @staticmethod
    async def open_connection(host, port, ssl=None):
        return _Reader(), _Writer()

    @staticmethod
    async def sleep(t):
        return None


class _FakeConn:
    """Records the raw lines the standalone sender writes."""

    last = None

    def __init__(self, reader, writer):
        self.lines = []
        _FakeConn.last = self

    async def raw(self, line):
        self.lines.append(line)

    async def pump(self, timeout, on_msg):
        return True

    async def close(self):
        return None


class _ScriptedConn:
    """A registration connection whose pump answers with ``collisions`` 433s."""

    def __init__(self, collisions):
        self.collisions = collisions
        self.lines = []

    async def raw(self, line):
        self.lines.append(line)

    async def pump(self, timeout, on_msg):
        for _ in range(self.collisions):
            result = await on_msg("433")
            if result is not None:
                return result
        return await on_msg("001")

    async def close(self):
        return None


# ── privmsg admission ──────────────────────────────────────────────────────

def privmsg_decision(r):
    a = bare()
    a._current_nick = r.get("nick", "")
    allowed = r.get("allowed_users") or []
    a._allowed_users_lower = {u.lower() for u in allowed if isinstance(u, str)}
    cap = {}

    async def fake_dispatch(text, chat_id, chat_type, user_id, user_name):
        cap.update({"dispatch": True, "text": text, "chat_id": chat_id,
                    "chat_type": chat_type, "user_id": user_id, "user_name": user_name})

    a._dispatch_message = fake_dispatch
    asyncio.run(ADAPTER._handle_privmsg(a, r.get("sender", ""), r.get("target", ""),
                                        r.get("text", "")))
    return cap if cap else dict(DROPPED)


def next_nick_collision(r):
    a = bare()
    a.nickname = r["nickname"]
    a._current_nick = r["current_nick"]
    sent = []

    async def go():
        async def fake_send(line):
            sent.append(line)
        a._send_raw = fake_send
        await ADAPTER._handle_line(a, ":srv 433 * %s :Nickname is already in use"
                                   % r["current_nick"])

    asyncio.run(go())
    return {"nick": a._current_nick, "command": sent[0] if sent else None}


# ── standalone registration ────────────────────────────────────────────────

def sa_register(base, collisions):
    saved = live._sa_error
    live._sa_error = lambda detail: {"detail": detail}
    try:
        conn = _ScriptedConn(collisions)
        err = asyncio.run(live._sa_register(conn, base, ""))
    finally:
        live._sa_error = saved
    nicks = [ln[len("NICK "):] for ln in conn.lines if ln.startswith("NICK ")]
    return {"ok": err is None,
            "nick": nicks[-1] if nicks else None,
            "detail": None if err is None else (err or {}).get("detail")}


def standalone_nick(r):
    return sa_register(r.get("nick_base", ""), int(r.get("attempts", 0)))["nick"]


def nick_retry(r):
    return sa_register(r.get("nick_base", ""), int(r.get("collisions", 0)))


def _sa_send(extra, chat_id, message):
    cfg = types.SimpleNamespace(extra=dict(extra))
    captured = {}
    saved = {k: getattr(live, k) for k in
             ("asyncio", "_StandaloneConn", "_sa_register", "t", "_ms_id",
              "send_error", "_get_scoped_secret")}
    _FakeConn.last = None

    async def fake_register(conn, nick_base, server_password):
        captured["nick_base"] = nick_base
        return None

    def fake_error(msg, **kw):
        return {"success": False, "error": str(msg)}

    live.asyncio = _Async
    live._StandaloneConn = _FakeConn
    live._sa_register = fake_register
    live.t = lambda key: "bye"
    live._ms_id = lambda: "MID"
    live.send_error = fake_error
    live._get_scoped_secret = lambda name, default=None, **kw: ({}).get(name, default)
    try:
        result = asyncio.run(live._standalone_send(cfg, chat_id, message))
    finally:
        for key, value in saved.items():
            setattr(live, key, value)

    conn = _FakeConn.last
    lines = list(conn.lines) if conn else []
    detail = None
    if isinstance(result, dict) and result.get("success") is False:
        detail = str(result.get("error", ""))
        if detail.startswith(SA_PREFIX):
            detail = detail[len(SA_PREFIX):]
    return {"lines": lines, "nick_base": captured.get("nick_base"), "detail": detail}


def _sent_target(lines):
    for ln in lines:
        if ln.startswith("JOIN "):
            return ln[len("JOIN "):]
        if ln.startswith("PRIVMSG "):
            return ln[len("PRIVMSG "):].split(" :", 1)[0]
    return None


def standalone_target(r):
    extra = {}
    for key in ("server", "channel", "port", "nickname"):
        if key in r:
            extra[key] = r[key]
    out = _sa_send(extra, r.get("chat_id", ""), r.get("message", "hi"))
    if out["detail"] is not None:
        return {"target": None, "nick_base": None, "error": out["detail"]}
    return {"target": _sent_target(out["lines"]), "nick_base": out["nick_base"],
            "error": None}


def standalone_lines(r):
    out = _sa_send({"server": "irc.example", "channel": "#fallback"},
                   r.get("target", ""), r.get("message", ""))
    payloads = [ln.split(" :", 1)[1] for ln in out["lines"] if ln.startswith("PRIVMSG ")]
    return {"lines": payloads, "error": out["detail"]}


# ── leaves ─────────────────────────────────────────────────────────────────

def is_channel(r):
    return live._is_irc_channel(r["target"])


def chat_info(r):
    return asyncio.run(ADAPTER.get_chat_info(bare(), r["chat_id"]))


def _with_secrets(env, fn):
    def look(name, default=None, **kw):
        return (env or {}).get(name, default)

    saved_live = live._get_scoped_secret
    saved_shared = shared.get_scoped_secret
    live._get_scoped_secret = look
    shared.get_scoped_secret = look
    try:
        return fn()
    finally:
        live._get_scoped_secret = saved_live
        shared.get_scoped_secret = saved_shared


def env_enablement(r):
    return _with_secrets(r.get("env") or {}, live._env_enablement)


def server_channel(r):
    cfg = types.SimpleNamespace(extra=dict(r.get("extra") or {}))
    server, channel = _with_secrets(r.get("env") or {}, lambda: live._server_channel(cfg))
    return {"server": server, "channel": channel}


def config_ready(r):
    cfg = types.SimpleNamespace(extra=dict(r.get("extra") or {}))
    return _with_secrets(r.get("env") or {}, lambda: live.validate_config(cfg))


HANDLERS = {
    "privmsg_decision": privmsg_decision,
    "next_nick_collision": next_nick_collision,
    "standalone_nick_base": lambda r: _sa_send(
        {"server": "irc.example", "channel": "#fallback", "nickname": r.get("nickname", "")},
        "#probe", "hi")["nick_base"],
    "standalone_nick": standalone_nick,
    "nick_retry": nick_retry,
    "standalone_target": standalone_target,
    "standalone_lines": standalone_lines,
    "is_channel": is_channel,
    "chat_info": chat_info,
    "env_enablement": env_enablement,
    "server_channel": server_channel,
    "config_ready": config_ready,
}


def expected(r):
    fn = HANDLERS.get(r.get("func"))
    if fn is None:
        raise AssertionError("unknown func %r" % (r.get("func"),))
    return fn(r)


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or cases == 0 else 0)


if __name__ == "__main__":
    main()
