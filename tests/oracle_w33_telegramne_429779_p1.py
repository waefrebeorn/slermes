#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_network.py
(gate w33_telegramne_429779_p1).

The live module is imported BY FILE PATH -- the telegram package __init__ drags
PTB in, and this host's application-control policy refuses the native
extensions behind it.  The seam stubs cover the import chain; nothing compared
here needs anything but the pure value functions.

Seams fed from the fixture:
  * os.environ        -> the module's HERMES_TELEGRAM_HTTP_* reads
  * sys.platform      -> getupdates_keepalive_connections
  * socket attributes -> tcp_keepalive_socket_options' availability list
  * logger records    -> the refused-value warning in telegram_verify

Compared values:
  env_number, ptb_request_kwargs, getupdates_keepalive, keepalive_options,
  proxy_target_hosts, bot_api_urls, verify_decision, classic_proxy_decision.
"""
import builtins
import importlib.util
import json
import logging
import os
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_telegramne_429779_p1"

_TMPHOME = tempfile.mkdtemp(prefix="w33-tn-p1-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

import httpx  # noqa: E402  real httpx first, so the stubs leave it alone
import socket  # noqa: E402

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

_SPEC = importlib.util.spec_from_file_location(
    "w33_telegram_network_live",
    QUARRY / "plugins" / "platforms" / "telegram" / "telegram_network.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live
_SPEC.loader.exec_module(live)

WANTED = {
    "env_number", "ptb_request_kwargs", "getupdates_keepalive", "keepalive_options",
    "proxy_target_hosts", "bot_api_urls", "verify_decision", "classic_proxy_decision",
}

_SENTINEL = object()


@contextmanager
def env_scope(values, clear_prefixes=()):
    snapshot = dict(os.environ)
    try:
        for prefix in clear_prefixes:
            for key in [k for k in os.environ if k.startswith(prefix)]:
                del os.environ[key]
        for key, value in (values or {}).items():
            os.environ[key] = value if isinstance(value, str) else str(value)
        yield
    finally:
        os.environ.clear()
        os.environ.update(snapshot)


@contextmanager
def platform_scope(value):
    saved = sys.platform
    try:
        sys.platform = value
        yield
    finally:
        sys.platform = saved


_MISSING = object()
_SOCKET_OPTIONS = ("TCP_KEEPIDLE", "TCP_KEEPALIVE", "TCP_KEEPINTVL", "TCP_KEEPCNT")


def keepalive_options(avail):
    """Patch socket's option table to the fixture's availability list, replay
    the live function, and decode the numeric tuples back to symbolic names."""
    saved = {}
    values = {}
    for index, name in enumerate(_SOCKET_OPTIONS):
        saved[name] = getattr(socket, name, _MISSING)
        if name in avail:
            current = saved[name]
            if current is _MISSING or current is None:
                current = 2000 + index
            setattr(socket, name, current)
            values[name] = current
        else:
            try:
                delattr(socket, name)
            except AttributeError:
                pass
    try:
        raw = live.tcp_keepalive_socket_options()
    finally:
        for name, old in saved.items():
            if old is _MISSING:
                try:
                    delattr(socket, name)
                except AttributeError:
                    pass
            else:
                setattr(socket, name, old)
    out = []
    for family, option, value in raw:
        if family == socket.SOL_SOCKET:
            out.append(["SOL_SOCKET", "SO_KEEPALIVE", value])
            continue
        name = next((n for n, v in values.items() if v == option), None)
        out.append(["IPPROTO_TCP", name, value])
    return out


class _Records(logging.Handler):
    def __init__(self):
        super().__init__()
        self.records = []

    def emit(self, record):
        self.records.append(record)


@contextmanager
def log_scope():
    handler = _Records()
    logger = live.logger
    saved_level = logger.level
    logger.addHandler(handler)
    logger.setLevel(logging.DEBUG)
    try:
        yield handler.records
    finally:
        logger.removeHandler(handler)
        logger.setLevel(saved_level)


def verify_decision(req):
    extra = req.get("extra")
    with log_scope() as records:
        result = live.telegram_verify(extra, _SENTINEL)
    warned = any(record.levelno >= logging.WARNING for record in records)
    return {"result": "default" if result is _SENTINEL else "classic", "warned": warned}


def classic_proxy_decision(req):
    verify = live._classic_key_exchange_context() if req.get("verify_is_classic") else object()
    proxy_url = req.get("proxy_url")
    if not req.get("is_str", True):
        proxy_url = 12345
    result = live.classic_proxy(proxy_url, verify)
    return {"result": "wrap" if isinstance(result, httpx.Proxy) else "passthrough"}


def expected(r):
    f = r.get("func")
    if f == "env_number":
        cast = int if r.get("cast") == "int" else float
        with env_scope(r.get("env") or {}):
            return live._env_number(r.get("key"), r.get("default"), cast)
    if f == "ptb_request_kwargs":
        with env_scope(r.get("env") or {}, clear_prefixes=("HERMES_TELEGRAM_HTTP_",)):
            return live.ptb_request_kwargs()
    if f == "getupdates_keepalive":
        with platform_scope(r.get("platform")):
            return live.getupdates_keepalive_connections()
    if f == "keepalive_options":
        return keepalive_options(r.get("avail") or [])
    if f == "proxy_target_hosts":
        return live.proxy_target_hosts(r.get("base_url"), r.get("fallback_ips") or [])
    if f == "bot_api_urls":
        return live.bot_api_urls(r.get("extra"))
    if f == "verify_decision":
        return verify_decision(r)
    if f == "classic_proxy_decision":
        return classic_proxy_decision(r)
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
