#!/usr/bin/env python3
"""sta_oracle_approval_transport.py — Python twin for
hermes_cli/approval_transport.py, mirrored by
src/hermes_cli/approval_transport.c.

Imports the LIVE module (stdlib-only imports: asyncio/hashlib/inspect/json/
logging/queue/threading/time/uuid) and drives the same ops as the C harness:

  {"kind":"create", ...}   -> allowed_choices, digest self-check, id length
  {"kind":"respond", ...}  -> decision binding fields
  {"kind":"invoke", ...}   -> normalized host result (choice/failure) with a
                              scripted sync present fn; the async path is a
                              Python-only concern (asyncio.run on the worker)
"""
import importlib.util
import json
import sys
from pathlib import Path

_DEV_ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "approval_transport_live",
    _DEV_ROOT / "hermes_cli" / "approval_transport.py")
at = importlib.util.module_from_spec(_spec)
sys.modules["approval_transport_live"] = at
_spec.loader.exec_module(at)


def mk_req(c):
    return at.ApprovalRequest.create(
        command=c.get("command", ""),
        description=c.get("description", ""),
        pattern_key=c.get("pattern_key", ""),
        pattern_keys=tuple(c.get("pattern_keys", [])),
        session_key=c.get("session_key", ""),
        surface=c.get("surface", ""),
        allow_session=bool(c.get("allow_session", False)),
        allow_permanent=bool(c.get("allow_permanent", False)),
        timeout_seconds=float(c.get("timeout_seconds", 300)),
    )


class FakePresent:
    def __init__(self, mode, polls_before_interrupt):
        self.mode = mode
        self.polls_left = polls_before_interrupt

    # sync present fn — returns ApprovalDecision or raises per script
    def __call__(self, request):
        m = self.mode
        if m == "sleep":
            import time
            time.sleep(2)
            return request.respond("once")
        if m == "raise":
            raise RuntimeError("transport exploded")
        if m == "wrong_type":
            return "not-a-decision"
        choice = "deny" if m == "ok_deny" else \
                 "maybe" if m == "bad_choice" else "once"
        if m == "stale_id":
            d = request.respond(choice)
            return at.ApprovalDecision(
                request_id="0" * 32, request_digest=d.request_digest,
                choice=choice)
        if m == "stale_digest":
            d = request.respond(choice)
            return at.ApprovalDecision(
                request_id=d.request_id, request_digest="deadbeef",
                choice=choice)
        return request.respond(choice)

    def on_poll(self):
        if self.polls_left > 0:
            self.polls_left -= 1

    def is_interrupted(self):
        return self.mode == "interrupt" and self.polls_left <= 0


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        o = {}
        if kind in ("create", "respond", "invoke"):
            req = mk_req(c)
            dm = (
                req.digest ==
                __import__("hashlib").sha256(
                    json.dumps({
                        "schema_version": 1,
                        "request_id": req.request_id,
                        "command": c.get("command", ""),
                        "description": c.get("description", ""),
                        "pattern_key": c.get("pattern_key", ""),
                        "pattern_keys": list(c.get("pattern_keys", [])),
                        "session_key": c.get("session_key", ""),
                        "surface": c.get("surface", ""),
                        "timeout_seconds": float(c.get("timeout_seconds", 300)),
                        "allowed_choices": list(req.allowed_choices),
                    }, sort_keys=True, separators=(",", ":")).encode()
                ).hexdigest()
            )
            if kind == "create":
                o = {
                    "allowed_choices": list(req.allowed_choices),
                    "digest_matches": dm,
                    "request_id_len": len(req.request_id),
                    "self_digest_ok": dm,
                }
            elif kind == "respond":
                d = req.respond(c.get("choice", ""))
                o = {
                    "request_id_matches": d.request_id == req.request_id,
                    "digest_matches": d.request_digest == req.digest,
                    "choice": d.choice,
                    "self_digest_ok": dm,
                }
            else:
                fp = FakePresent(c.get("present", "ok_once"),
                                 int(c.get("polls_before_interrupt", -1)))
                r = at.invoke_approval_transport(
                    fp, req,
                    timeout_seconds=float(c.get("timeout_seconds", 5.0)),
                    poll_interval=float(c.get("poll_interval", 0.02)),
                    on_poll=fp.on_poll,
                    is_interrupted=fp.is_interrupted)
                if c.get("present") == "sleep":
                    import time
                    time.sleep(2)
                o = {
                    "choice": r.choice,
                    "failure": r.failure or "",
                    "active_workers_after":
                        at._transport_worker_slots._value,
                    "self_digest_ok": dm,
                }
        print(json.dumps(o, ensure_ascii=False))


if __name__ == "__main__":
    main()
