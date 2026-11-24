#!/usr/bin/env python3
"""Differential oracle for agent/pet/manifest.py (gate w23_manifest_27beb4).

The module fetches the public petdex manifest over httpx through the SSRF-safe
client and memoises it for five minutes.  Network, the SSRF policy and the wall
clock are seams, so the live functions are run against a scripted transport:

  * ``httpx`` and ``tools.url_safety`` are stub modules: ``is_safe_url`` answers
    the fixture's verdict and ``create_ssrf_safe_client`` hands back a client
    whose ``get``/``raise_for_status``/``json`` answer the fixture's body or
    raise its error text.  Setting ``httpx`` to ``None`` in ``sys.modules`` makes
    the function's own ``import httpx`` fail, exercising the ImportError branch;
  * the module cache is seeded from the fixture so the warm/force decision is real
    (whether the transport was touched at all is observed, not assumed);
  * ``find_entry`` runs live over a stub ``fetch_manifest`` returning the
    fixture's catalogue.

``ManifestEntry.from_dict`` -- the str()/or coercion of every column -- and the
eligibility filter are live.
"""
import dataclasses
import importlib.util
import json
import sys
import time
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "pet" / "manifest.py"
GATE = "w23_manifest_27beb4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("pet_manifest_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["pet_manifest_live"] = live
_spec.loader.exec_module(live)

WANTED = {"entry_from_dict", "fetch", "find_entry", "consts"}


def as_dict(entry):
    return dataclasses.asdict(entry)


class _Response:
    def __init__(self, world):
        self._world = world

    def raise_for_status(self):
        if self._world.get("raise"):
            raise RuntimeError(self._world["raise"])

    def json(self):
        return self._world.get("payload")


class _Client:
    def __init__(self, world, hits):
        self._world = world
        self._hits = hits

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def get(self, url, headers=None):
        self._hits["get"] += 1
        if self._world.get("error"):
            raise RuntimeError(self._world["error"])
        return _Response(self._world)


def run_fetch(world):
    hits = {"get": 0}
    live.clear_cache()
    cached = world.get("cached") or {}
    if cached.get("warm"):
        rows = [live.ManifestEntry.from_dict(e) for e in (cached.get("entries") or [])
                if isinstance(e, dict)]
        live._cache = (time.monotonic(), rows)

    url_safety = types.ModuleType("tools.url_safety")
    url_safety.is_safe_url = lambda _url: bool(world.get("safe", True))
    url_safety.create_ssrf_safe_client = lambda **_kw: _Client(world, hits)

    httpx_stub = None if world.get("import_ok") is False else types.ModuleType("httpx")
    saved_httpx = sys.modules.get("httpx")
    saved_safety = sys.modules.get("tools.url_safety")
    sys.modules["httpx"] = httpx_stub
    sys.modules["tools.url_safety"] = url_safety
    try:
        entries = live.fetch_manifest(timeout=live._DEFAULT_TIMEOUT,
                                      force=bool(world.get("force")))
    except live.ManifestError as exc:
        return {"ok": False, "error": str(exc)}
    finally:
        if saved_httpx is None:
            sys.modules.pop("httpx", None)
        else:
            sys.modules["httpx"] = saved_httpx
        if saved_safety is None:
            sys.modules.pop("tools.url_safety", None)
        else:
            sys.modules["tools.url_safety"] = saved_safety
        live.clear_cache()
    return {"ok": True, "from_cache": hits["get"] == 0,
            "entries": [as_dict(e) for e in entries]}


def run_find(req):
    rows = [live.ManifestEntry.from_dict(x) for x in (req.get("entries") or [])
            if isinstance(x, dict)]
    saved = live.fetch_manifest
    live.fetch_manifest = lambda **_kw: rows
    try:
        hit = live.find_entry(req.get("slug"), timeout=live._DEFAULT_TIMEOUT)
    finally:
        live.fetch_manifest = saved
    return {"found": hit is not None, "entry": as_dict(hit) if hit is not None else None}


def expected(r):
    f = r.get("func")
    if f == "consts":
        return {"url": live.MANIFEST_URL, "ttl": live._MANIFEST_TTL,
                "default_timeout": live._DEFAULT_TIMEOUT}
    if f == "entry_from_dict":
        return as_dict(live.ManifestEntry.from_dict(r.get("data") if isinstance(r.get("data"), dict) else {}))
    if f == "fetch":
        return run_fetch(r)
    if f == "find_entry":
        return run_find(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
