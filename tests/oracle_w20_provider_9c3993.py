#!/usr/bin/env python3
"""Differential oracle for plugins/web/keenable/provider.py
(w20_provider_9c3993).

The live provider runs against the real ``plugins.web._common`` result shapes;
``requests``, the provider env, the interrupt flag and the keyless-ring
hand-off are scripted seams. Compared values: the header dict, the keyed POST
the provider builds, the mapped search result and the per-URL extract entries
(including Keenable's origin-error translation).
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class WebSearchProvider:
    pass


ENV = {}
KEYLESS = {"on": False}
POST = {}
GET = {}
REPLY = {}


def _provider_env(name):
    return ENV.get(name, "")


_mk("agent")
_mk("agent.ssl_verify", requests_verify=lambda: True)
_mk("agent.web_search_provider", WebSearchProvider=WebSearchProvider,
    get_provider_env=_provider_env)
_mk("tools")
_mk("tools.interrupt", is_interrupted=lambda: False)
_mk("plugins.web.keyless_mcp", use_keyless=lambda name, key: KEYLESS["on"],
    search_with_failover=lambda *a, **k: {"success": True, "data": {"web": []}},
    extract_with_failover=lambda *a, **k: [])


class Resp:
    def __init__(self, spec):
        self.spec = spec if isinstance(spec, dict) else {}
        self.status_code = self.spec.get("status", 200)
        self.text = self.spec.get("text", "")

    def json(self):
        if self.spec.get("json_error"):
            raise ValueError(self.spec["json_error"])
        body = self.spec.get("body")
        return body if body is not None else {}


def _post(url, json=None, headers=None, timeout=None, verify=None):
    POST.clear()
    POST.update({"url": url, "json": json, "headers": headers})
    return Resp(REPLY)


def _get(url, params=None, headers=None, timeout=None, verify=None):
    target = (params or {}).get("url", url)
    spec = GET.get(target) or {}
    if spec.get("get_error"):
        raise ConnectionError(spec["get_error"])
    return Resp(spec)


_mk("requests", post=_post, get=_get)

NAME = "w20_provider_9c3993_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "plugins", "web", "keenable", "provider.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

provider = live.KeenableWebSearchProvider()


def expected(r):
    f = r.get("func")
    ENV["KEENABLE_API_KEY"] = r.get("api_key", "")
    KEYLESS["on"] = bool(r.get("keyless"))
    if f == "headers":
        return live._keenable_headers(r.get("api_key", ""))
    if f == "search_request":
        POST.clear()
        REPLY.clear()
        REPLY.update(r.get("reply") or {})
        if KEYLESS["on"]:
            provider.search(r.get("query", ""), int(r.get("limit", 5)))
            return {"keyless": True}
        provider.search(r.get("query", ""), int(r.get("limit", 5)))
        return {"keyless": False, "url": POST["url"], "body": POST["json"],
                "headers": POST["headers"]}
    if f == "search_result":
        ENV["KEENABLE_API_KEY"] = r.get("api_key", "k")
        KEYLESS["on"] = False
        REPLY.clear()
        REPLY.update(r.get("reply") or {})
        return provider.search(r.get("query", "q"), int(r.get("limit", 5)))
    if f == "extract":
        ENV["KEENABLE_API_KEY"] = r.get("api_key", "k")
        KEYLESS["on"] = False
        GET.clear()
        for url, spec in zip(r.get("urls") or [], r.get("replies") or []):
            GET[url] = spec
        return provider.extract(list(r.get("urls") or []))
    if f == "setup_schema":
        return provider.get_setup_schema()
    if f == "identity":
        return {"name": provider.NAME, "display_name": provider.DISPLAY_NAME,
                "key_env": provider.KEY_ENV, "extract": provider.EXTRACT,
                "keyless": provider.KEYLESS}
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
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_provider_9c3993: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
