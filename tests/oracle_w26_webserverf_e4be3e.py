#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_file_tickets.py (gate w26_webserverf_e4be3e).

The live ticket store is driven as a value machine: ``_tickets`` is replaced with
the fixture's store, ``time`` with a fixed monotonic clock, ``secrets`` with a
fixed token generator, the TTLs/``_MAX_TICKETS`` with the fixture's caps, and
``browser_credential`` with an identity generation plus a fixture-supplied
``current`` map.  Requests are synthetic objects carrying exactly the attributes
``redeem`` reads, so the compared value is the store after the decision: which
scope matched, whether a use was spent, and whether a generation revoked it.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_webserverf_e4be3e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(REPO.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(REPO.parent)

from hermes_cli import web_server_file_tickets as live  # noqa: E402

WANTED = {"scope", "mint", "redeem", "constants"}

TTL_DOWNLOAD = live.DOWNLOAD_TTL_SECONDS
TTL_STREAM = live.STREAM_TTL_SECONDS
MAX_TICKETS = live._MAX_TICKETS


class _Query:
    def __init__(self, items):
        self._items = [tuple(p) for p in items]

    def get(self, key, default=""):
        for k, v in self._items:
            if k == key:
                return v
        return default

    def multi_items(self):
        return list(self._items)


class _Request:
    def __init__(self, method, path, items):
        self.method = method
        self.url = SimpleNamespace(path=path)
        self.query_params = _Query(items)
        self.state = SimpleNamespace()


def _authority():
    return live.FileAuthority(target=Path("ticket.bin"), homes=())


def _entry(raw):
    return live._Ticket(
        scope=(raw["route"], tuple(tuple(p) for p in raw.get("query") or [])),
        expires_at=raw["expires_at"],
        uses_left=raw.get("uses_left"),
        authority=_authority(),
        generation=raw.get("generation"),
    )


def _store_out(store):
    return {
        key: {
            "route": entry.scope[0],
            "query": [list(p) for p in entry.scope[1]],
            "expires_at": entry.expires_at,
            "uses_left": entry.uses_left,
            "generation": entry.generation,
        }
        for key, entry in store.items()
    }


def _patch_clock(now):
    live.time = SimpleNamespace(monotonic=lambda: now)


def scope(req):
    return live._scope(req.get("route_path", ""), [tuple(p) for p in (req.get("query") or [])])


def scope_json(req):
    route, pairs = scope(req)
    return {"route": route, "query": [list(p) for p in pairs]}


def mint(req):
    now = req.get("now", 0.0)
    live._tickets = {key: _entry(raw) for key, raw in (req.get("store") or {}).items()}
    live.time = SimpleNamespace(monotonic=lambda: now)
    live.secrets = SimpleNamespace(token_urlsafe=lambda n: req.get("token", ""))
    live.DOWNLOAD_TTL_SECONDS = req.get("ttl_download", TTL_DOWNLOAD)
    live.STREAM_TTL_SECONDS = req.get("ttl_stream", TTL_STREAM)
    live._MAX_TICKETS = req.get("max_tickets", MAX_TICKETS)
    live.browser_credential = SimpleNamespace(generation_of=lambda token: token,
                                              is_current=lambda gen: False)
    token = live.mint(req.get("route", ""), req.get("query") or {},
                      authority=_authority(), session_token=req.get("session_token"))
    return {"ticket": token, "store": _store_out(live._tickets)}


def redeem(req):
    now = req.get("now", 0.0)
    live._tickets = {key: _entry(raw) for key, raw in (req.get("store") or {}).items()}
    _patch_clock(now)
    current = req.get("current") or {}
    live.browser_credential = SimpleNamespace(generation_of=lambda token: token,
                                              is_current=lambda gen: bool(current.get(gen)))
    request = _Request(req.get("method", "GET"), req.get("path", ""), req.get("query") or [])
    ok = live.redeem(request)
    return {"ok": bool(ok), "store": _store_out(live._tickets),
            "authority": hasattr(request.state, "file_ticket_authority")}


def constants(req):
    return {
        "download_ttl_seconds": TTL_DOWNLOAD,
        "download_max_redemptions": live.DOWNLOAD_MAX_REDEMPTIONS,
        "stream_ttl_seconds": TTL_STREAM,
        "max_tickets": MAX_TICKETS,
        "routes": dict(live.ROUTES),
    }


FUNCS = {"scope": scope_json, "mint": mint, "redeem": redeem, "constants": constants}


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
