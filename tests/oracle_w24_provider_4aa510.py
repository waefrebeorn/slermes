#!/usr/bin/env python3
"""Differential oracle for plugins/web/tavily/provider.py (gate w24_provider_4aa510).

The live provider module is imported and its value layer is called directly.
The two seams are supplied as values so no network and no config is touched:

  * ``_tavily_headers`` -- keyed vs keyless headers;
  * the ``search`` payload build (``min(limit, SEARCH_LIMIT_CAP)`` + the two
    include flags), which is the only thing ``search`` decides before POSTing;
  * ``_normalize_tavily_search_results`` / ``_normalize_tavily_documents`` --
    the shared search/extract shapes, in their contract key order;
  * ``_missing_key_error`` and ``_auth`` (``provider_env`` / ``use_keyless`` are
    patched onto the module from the fixture's api_key/keyless values);
  * ``get_setup_schema`` and the class/module literals.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_provider_4aa510"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.web.tavily import provider as live  # noqa: E402

WANTED = {"headers", "search_payload", "normalize_search", "normalize_documents",
          "missing_key_error", "auth", "setup_schema", "constants"}

CLS = live.TavilyWebSearchProvider


def _typed(fn, *args):
    """Name the exception the source raises on a bad shape."""
    try:
        return fn(*args)
    except (AttributeError, TypeError) as exc:
        return {"error": type(exc).__name__}


def search_payload(req):
    limit = req.get("limit")
    try:
        payload = {"query": req.get("query"), "max_results": min(limit, live.SEARCH_LIMIT_CAP),
                   **live._SEARCH_PAYLOAD}
    except TypeError as exc:
        return {"error": type(exc).__name__}
    return payload


def auth(req):
    live.provider_env = lambda name: req.get("api_key")
    live.use_keyless = lambda name, api_key: bool(req.get("keyless"))
    key, missing, prefix = live._auth(req.get("action"))
    return {"key": key, "missing": missing, "prefix": prefix}


def expected(r):
    f = r.get("func")
    if f == "headers":
        return live._tavily_headers(r.get("api_key"))
    if f == "search_payload":
        return search_payload(r)
    if f == "normalize_search":
        return _typed(live._normalize_tavily_search_results, r.get("response"))
    if f == "normalize_documents":
        return _typed(live._normalize_tavily_documents, r.get("response"))
    if f == "missing_key_error":
        return live._missing_key_error(r.get("action"))
    if f == "auth":
        return auth(r)
    if f == "setup_schema":
        return CLS().get_setup_schema()
    if f == "constants":
        return {"NAME": CLS.NAME, "DISPLAY_NAME": CLS.DISPLAY_NAME, "KEY_ENV": CLS.KEY_ENV,
                "EXTRACT": CLS.EXTRACT, "KEYLESS": CLS.KEYLESS,
                "_CLIENT_NAME": live._CLIENT_NAME, "_SEARCH_PAYLOAD": dict(live._SEARCH_PAYLOAD),
                "SEARCH_LIMIT_CAP": live.SEARCH_LIMIT_CAP}
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
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
