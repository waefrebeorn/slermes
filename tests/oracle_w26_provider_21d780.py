#!/usr/bin/env python3
"""Differential oracle for plugins/web/ddgs/provider.py (gate w26_provider_21d780).

The live provider module is imported and driven through its two value seams:
``_parse_envelope(raw, proc)`` with a stub process whose ``poll()`` is the fixed
exit code, and ``DDGSWebSearchProvider.search`` with ``_run_ddgs_search_bounded``
replaced by a fixture that obeys the requested seam (ok/timeout/interrupt/error)
and records the ``safe_limit`` it was handed.  The picker entry and the module
constants are compared as plain values.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_provider_21d780"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

from plugins.web.ddgs import provider as live  # noqa: E402

WANTED = {"parse_envelope", "search", "setup_schema", "constants"}


class _Proc:
    """The worker process seam: only the exit code is ever read."""

    def __init__(self, code):
        self._code = code

    def poll(self):
        return self._code


def parse_envelope(req):
    try:
        results = live._parse_envelope(req.get("raw"), _Proc(req.get("exit_code", 0)))
    except Exception as exc:  # noqa: BLE001
        return {"error": str(exc)}
    return {"ok": True, "results": results}


def search(req):
    seam = req.get("seam", "ok")
    hits = req.get("hits")
    seen = {}

    def fake(query, safe_limit):
        seen["safe_limit"] = safe_limit
        if seam == "timeout":
            raise TimeoutError("bounded")
        if seam == "interrupt":
            raise live._SearchInterrupted("bounded")
        if seam == "error":
            raise RuntimeError("boom")
        return list(hits or [])

    live._run_ddgs_search_bounded = fake
    prov = live.DDGSWebSearchProvider()
    prov.is_available = lambda: bool(req.get("available"))
    try:
        result = prov.search(req.get("query") or "", req.get("limit"))
    except Exception as exc:  # noqa: BLE001
        return {"result": {"error": type(exc).__name__}, "safe_limit": seen.get("safe_limit")}
    return {"result": result, "safe_limit": seen.get("safe_limit")}


def setup_schema(req):
    return live.DDGSWebSearchProvider().get_setup_schema()


def constants(req):
    return {
        "search_timeout_secs": live._SEARCH_TIMEOUT_SECS,
        "poll_interval_secs": live._POLL_INTERVAL_SECS,
        "terminate_grace_secs": live._TERMINATE_GRACE_SECS,
        "name": live.DDGSWebSearchProvider.NAME,
        "display_name": live.DDGSWebSearchProvider.DISPLAY_NAME,
    }


FUNCS = {"parse_envelope": parse_envelope, "search": search,
         "setup_schema": setup_schema, "constants": constants}


def expected(req):
    try:
        return FUNCS[req["func"]](req)
    except Exception as exc:  # noqa: BLE001
        return {"error": type(exc).__name__}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        py = expected(req)
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
