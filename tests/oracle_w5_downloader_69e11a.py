#!/usr/bin/env python3
"""Differential oracle for pm/downloader.py pure helpers (w5_downloader_69e11a).

pm.network is stubbed; response headers are injected from the fixture; C
exception paths return {"__error__": str(exc)} and the live side does the same.
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


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("pm")
_stub("pm.network", edge_block=lambda exc: None, is_transient=lambda exc: False,
      retry_network=lambda fn, **kw: fn())
sys.modules["pm"].network = sys.modules["pm.network"]

LIVE_NAME = "w5_downloader_69e11a_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/pm/downloader.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def _pairs(value):
    return [(int(p[0]), int(p[1])) for p in (value or [])]


class _Response:
    def __init__(self, status, headers):
        self.status = status
        self.headers = headers


def _source(r):
    return live.Source(url=r.get("url", ""), dest="dest",
                       sha256=r.get("sha256", ""), http_mirror=r.get("http_mirror", ""))


def compute(r):
    f = r.get("func")
    if f == "pinned":
        return live._pinned(r.get("sha256", ""))
    if f == "host":
        return live._host(r.get("url", ""))
    if f == "loopback":
        return live._loopback(r.get("url", ""))
    if f == "coalesce":
        return [list(x) for x in live._coalesce(_pairs(r.get("ranges")))]
    if f == "missing":
        return [list(x) for x in live._missing(int(r.get("total") or 0), _pairs(r.get("covered")))]
    if f == "source_plain_http":
        return _source(r).plain_http()
    if f == "source_byte_limit":
        return _source(r).byte_limit()
    if f == "validate_range":
        headers = {"Content-Range": r.get("content_range", "")}
        if r.get("content_length") is not None:
            headers["Content-Length"] = r["content_length"]
        if r.get("content_encoding") is not None:
            headers["Content-Encoding"] = r["content_encoding"]
        resp = _Response(int(r.get("status", 0)), headers)
        try:
            return live._validate_range(resp, int(r.get("start", 0)), int(r.get("end", 0)),
                                        None if r.get("total") is None else int(r["total"]))
        except Exception as exc:
            return {"__error__": str(exc)}
    if f == "strong_etag":
        return live._strong_etag(_Response(200, {"ETag": r.get("etag", "")}))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH downloader %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_downloader_69e11a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
