#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_network.py
(gate w33_telegramne_429779_p2).

The live module is imported BY FILE PATH (the telegram package __init__ drags
PTB, whose native extensions this host refuses); the seam stubs cover that
import chain, and real ``httpx`` is imported first so the compared values come
from the real Limits / Request / exception classes.

Seams fed from the fixture:
  * the DoH HTTP response (body + status) via a fake streaming client;
  * RLIMIT_NOFILE (``_nofile_soft_limit``) and the platform Limits object;
  * ``sys.platform`` and ``time.monotonic`` (the walk's elapsed seconds);
  * the proxy/SSL identities (booleans) and the sticky-path sentinel.

Compared values:
  normalize_fallback_ips, cap_fallback_ips, parse_fallback_ip_env,
  is_retryable, attempt_order, connect_share, request_connect_budget,
  rewrite_for_ip, doh_answer_ips, discover_merge, pool_plan, walk_failed.
"""
import asyncio
import builtins
import importlib
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
GATE = "w33_telegramne_429779_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w33-tn-p2-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

import httpx  # noqa: E402  real httpx first, so the stubs leave it alone

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
    "normalize_fallback_ips", "cap_fallback_ips", "parse_fallback_ip_env",
    "is_retryable", "attempt_order", "connect_share", "request_connect_budget",
    "rewrite_for_ip", "doh_answer_ips", "discover_merge", "pool_plan", "walk_failed",
}


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


@contextmanager
def monotonic_scope(value):
    saved = live.time.monotonic
    live.time.monotonic = lambda: value
    try:
        yield
    finally:
        live.time.monotonic = saved


def _real(name):
    return getattr(httpx, name, None) or getattr(builtins, name, None)


def build_exception(mro):
    """The exception class the fixture's MRO describes, built on real bases."""
    real_start = next((i for i, name in enumerate(mro) if _real(name) is not None), None)
    if real_start is None:
        cls = Exception
        fabricated = list(mro)
    else:
        cls = _real(mro[real_start])
        fabricated = list(mro[:real_start])
    for name in reversed(fabricated):
        cls = type(name, (cls,), {})
    exc = cls("boom")
    actual = [c.__name__ for c in type(exc).__mro__]
    assert actual[:len(mro)] == list(mro), (actual, mro)
    return exc


class _Resp:
    def __init__(self, status, body):
        self.status = status
        self.body = body

    def raise_for_status(self):
        if self.status >= 400:
            raise RuntimeError("HTTP %d" % self.status)

    async def aiter_raw(self):
        yield self.body


class _Stream:
    def __init__(self, status, body):
        self.status = status
        self.body = body

    async def __aenter__(self):
        return _Resp(self.status, self.body)

    async def __aexit__(self, *exc):
        return False


class _Client:
    def __init__(self, status, body):
        self.status = status
        self.body = body

    def stream(self, *args, **kwargs):
        return _Stream(self.status, self.body)


class _Records(logging.Handler):
    def __init__(self):
        super().__init__()
        self.records = []

    def emit(self, record):
        self.records.append(record)


def pool_plan(req):
    """Replay build_ptb_requests against the fixture's env / platform / fd budget
    and read the two request classes' derived limits back out."""
    captured = []

    class Request:
        def __init__(self, **kwargs):
            captured.append(kwargs)

    base = req.get("base")
    soft = req.get("soft")
    limits_mod = importlib.import_module("gateway.platforms._http_client_limits")
    pool_mod = sys.modules[live.fd_bounded_limits.__module__]
    saved_limits = limits_mod.platform_httpx_limits
    saved_soft = pool_mod._nofile_soft_limit

    def fake_limits():
        if base is None:
            return None
        return httpx.Limits(max_keepalive_connections=base.get("max_keepalive_connections"),
                            keepalive_expiry=base.get("keepalive_expiry"))

    limits_mod.platform_httpx_limits = fake_limits
    pool_mod._nofile_soft_limit = lambda: soft
    try:
        with env_scope(req.get("env") or {},
                       clear_prefixes=("HERMES_TELEGRAM_HTTP_", "HERMES_GATEWAY_HTTPX_")):
            with platform_scope(req.get("platform")):
                live.build_ptb_requests(Request, fallback_ips=list(req.get("fallback_ips") or []),
                                        proxy_url=req.get("proxy_url"), verify=object())
    finally:
        limits_mod.platform_httpx_limits = saved_limits
        pool_mod._nofile_soft_limit = saved_soft

    def limits_of(kwargs):
        limits = kwargs.get("httpx_kwargs", {}).get("limits")
        if limits is None:
            return None
        return {"max_connections": limits.max_connections,
                "max_keepalive_connections": limits.max_keepalive_connections,
                "keepalive_expiry": limits.keepalive_expiry}

    first = captured[0]
    httpx_kwargs = first.get("httpx_kwargs", {})
    transport = httpx_kwargs.get("transport")
    return {
        "kwargs": {key: first[key] for key in ("connection_pool_size", "pool_timeout",
                                               "connect_timeout", "read_timeout",
                                               "write_timeout", "media_write_timeout")},
        "ips": list(getattr(transport, "_fallback_ips", [])),
        "pool": limits_of(captured[0]),
        "updates": limits_of(captured[1]),
        "route": "transport" if "transport" in httpx_kwargs else "mounts",
    }


def expected(r):
    f = r.get("func")
    if f == "normalize_fallback_ips":
        return live._normalize_fallback_ips(r.get("values") or [])
    if f == "cap_fallback_ips":
        return live._cap_fallback_ips(r.get("values") or [])
    if f == "parse_fallback_ip_env":
        return live.parse_fallback_ip_env(r.get("value"))
    if f == "is_retryable":
        return live._is_retryable_connect_error(build_exception(r.get("mro") or []))
    if f == "attempt_order":
        sticky = r.get("sticky") or {}
        transport = live.TelegramFallbackTransport.__new__(live.TelegramFallbackTransport)
        transport._sticky_ip = sticky.get("value") if sticky.get("set") else live._UNSET
        transport._fallback_ips = list(r.get("fallback_ips") or [])
        return transport._attempt_order()
    if f == "connect_share":
        transport = live.TelegramFallbackTransport.__new__(live.TelegramFallbackTransport)
        transport._connect_budget = r.get("budget")
        order = [None] * int(r.get("paths") or 1)
        with monotonic_scope(float(r.get("elapsed") or 0.0)):
            return transport._connect_share(order, int(r.get("index") or 0), 0.0)
    if f == "request_connect_budget":
        ext = r.get("ext")
        if ext == "dict":
            timeout = {}
            if r.get("connect") is not None:
                timeout["connect"] = r.get("connect")
            request = httpx.Request("GET", "https://api.telegram.org/x",
                                    extensions={"timeout": timeout})
        elif ext == "none":
            request = httpx.Request("GET", "https://api.telegram.org/x")
        else:
            request = httpx.Request("GET", "https://api.telegram.org/x",
                                    extensions={"timeout": "soon"})
        out = live._request_with_connect_budget(request, r.get("budget"))
        timeout = out.extensions.get("timeout")
        return {"changed": out is not request,
                "connect": timeout.get("connect") if isinstance(timeout, dict) else None}
    if f == "rewrite_for_ip":
        request = httpx.Request("GET", r.get("url"))
        out = live._rewrite_request_for_ip(request, r.get("ip"))
        return {"url": str(out.url), "host_header": out.headers.get("host"),
                "sni": out.extensions.get("sni_hostname")}
    if f == "doh_answer_ips":
        body = str(r.get("body") or "").encode("utf-8")
        status = int(r.get("status") or 200)
        return asyncio.run(live._query_doh_provider(
            _Client(status, body),
            {"url": "https://dns.google/resolve", "params": {}, "headers": {}}))
    if f == "discover_merge":
        doh_ips = r.get("doh_ips") or []
        seeds = r["seeds"] if "seeds" in r else live.SEED_FALLBACK_IPS
        validated = live._normalize_fallback_ips(list(dict.fromkeys(doh_ips)))
        if validated:
            return list(dict.fromkeys([*validated, *seeds]))
        return list(seeds)
    if f == "pool_plan":
        return pool_plan(r)
    if f == "walk_failed":
        failures = [tuple(pair) for pair in (r.get("failures") or [])]
        attempted = int(r.get("attempted") or 0)
        order_len = int(r.get("order_len") or 0)
        reason = ("unreachable via all paths" if attempted == order_len
                  else "connect budget exhausted before all paths were tried")
        handler = _Records()
        logger = live.logger
        saved_level = logger.level
        logger.addHandler(handler)
        logger.setLevel(logging.DEBUG)
        try:
            live._warn_walk_failed(reason, failures)
        finally:
            logger.removeHandler(handler)
            logger.setLevel(saved_level)
        message = handler.records[0].getMessage() if handler.records else ""
        return {"reason": reason, "message": message}
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
