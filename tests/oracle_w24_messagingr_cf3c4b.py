#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/messaging_relay.py
(gate w24_messagingr_cf3c4b).

The live router module is imported and its real handlers are driven: the gateway
control-socket call is the seam, so ``gateway.control_socket.query_gateway_control``
is answered from the fixture (a value, or the "no answer" None) and the outcome is
read off what production code does with it -- the returned payload for 200, the
HTTPException's status/detail for 503 and 409.  No socket, no gateway, no thread
of ours: ``_call`` still runs its own ``asyncio.to_thread``.
"""
import asyncio
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_messagingr_cf3c4b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from fastapi import HTTPException  # noqa: E402

# The gateway home is a seam of the transport, not of the decision: give the
# process a private home so get_process_hermes_home() never guesses.
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w24mr_home_"))

import gateway.control_socket as control  # noqa: E402
from hermes_cli.web_routers import messaging_relay as live  # noqa: E402

WANTED = {"ask", "params", "not_ready", "constants"}
SEEN = {}


def _answer_with(value):
    def spy(home, verb, params=None, timeout=None):
        SEEN["params"] = params
        return value
    return spy


def ask(req):
    control.query_gateway_control = _answer_with(req.get("answer"))
    try:
        return {"status": 200, "body": live._ask_gateway({"action": "status"})}
    except HTTPException as exc:
        return {"status": exc.status_code, "body": exc.detail}
    except AttributeError as exc:
        return {"error": str(exc)}


async def _drive(action, platform, code, user):
    if action == "status":
        return await live.get_relay_links()
    if action == "request":
        return await live.request_relay_link(platform)
    if action == "state":
        return await live.get_relay_link_state(code)
    if action == "unlink":
        return await live.unlink_relay_account(platform, user)
    raise AssertionError("unknown action %r" % (action,))


def params(req):
    SEEN.clear()
    control.query_gateway_control = _answer_with({"ok": True})
    action = req.get("action")
    try:
        asyncio.run(_drive(action, req.get("platform"), req.get("code"),
                           req.get("platform_user_id")))
    except AssertionError:
        return None
    return SEEN.get("params")


def expected(req):
    func = req.get("func")
    if func == "ask":
        return ask(req)
    if func == "params":
        return params(req)
    if func == "not_ready":
        return req.get("code") in live._NOT_READY
    if func == "constants":
        return {
            "not_ready": sorted(live._NOT_READY),
            "routes": [{"method": sorted(route.methods)[0], "path": route.path}
                       for route in live.router.routes],
        }
    raise AssertionError("unknown func %r" % (func,))


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
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
