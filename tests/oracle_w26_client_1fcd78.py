#!/usr/bin/env python3
"""Differential oracle for tools/connectors/portal/client.py
(gate w26_client_1fcd78).

The live module is imported; validate_slug and _route_key are called directly.
authorization_token runs on a PortalConnectorClient whose transport, endpoint
resolver and header provider are injected -- the same three seams the class
takes -- so the header set is the fixture's and no request is made.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_client_1fcd78"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from tools.connectors.portal import client as live  # noqa: E402
from tools.connectors.gateway.errors import GatewayAuthError  # noqa: E402
from tools.connectors.portal.errors import InvalidConnectorSlug  # noqa: E402

WANTED = {"validate_slug", "route_key", "auth_token", "constants"}


def client_with(headers):
    return live.PortalConnectorClient(
        transport=object(),
        endpoint_resolver=lambda: "https://portal.invalid",
        header_provider=lambda _url: dict(headers),
    )


def expected(req):
    f = req.get("func")
    if f == "validate_slug":
        try:
            live.validate_slug(req.get("slug"))
            return {"valid": True, "error": None}
        except InvalidConnectorSlug:
            return {"valid": False, "error": "InvalidConnectorSlug"}
        except TypeError:
            return {"valid": False, "error": "TypeError"}
    if f == "route_key":
        path = req.get("path")
        return live._route_key(path if isinstance(path, str) else "")
    if f == "auth_token":
        headers = req.get("headers")
        headers = headers if isinstance(headers, dict) else {}
        try:
            return {"token": client_with(headers).authorization_token(), "error": None}
        except GatewayAuthError:
            return {"token": None, "error": "GatewayAuthError"}
    if f == "constants":
        return {
            "default_timeout_seconds": live.DEFAULT_TIMEOUT_SECONDS,
            "slug_pattern": live._SLUG_RE.pattern,
            "route_words": sorted(live._PORTAL_ROUTE_WORDS),
        }
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
