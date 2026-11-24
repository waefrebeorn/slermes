#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/messaging.py — gate w28_messaging_ede80b_p2.

Telegram QR onboarding.  The live module is imported BY FILE PATH (its package
__init__ drags the whole web stack).  Three of the six checks call the real
handlers with only their I/O seams stubbed:

  * ``start_telegram_onboarding`` — the pairing-service POST is a canned dict;
  * ``get_telegram_onboarding_status`` — a record is placed in the real pairing
    table and the polled payload is canned;
  * ``apply_telegram_onboarding`` — ``save_env_value`` records what was written,
    so the returned "value" is the ids the handler put in TELEGRAM_ALLOWED_USERS;
  * ``_normalize_telegram_user_id`` / ``_telegram_ready_payload`` are called
    straight;
  * ``_parse_expiry_ts`` is called straight, and the +600 s clock fallback (the
    one branch that is not a function of the input) is reported as null.
"""
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
import time
from datetime import UTC, datetime
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_messaging_ede80b_p2"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "messaging.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# This host blocks pydantic's native extension and the router imports fastapi
# at module scope: those are seams, not logic under test.
sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only; must NOT shadow the quarry packages
from hermes_seam_stubs import install as _install_seams  # noqa: E402
_install_seams()

_spec = importlib.util.spec_from_file_location("msg_live_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

HTTPException = live.HTTPException
ORIG = {name: getattr(live, name) for name in
        ("_telegram_onboarding_request", "_telegram_record_or_404", "_profile_scope",
         "save_env_value", "_write_platform_enabled", "_restart_gateway_after")}

WANTED = {"normalize_user_id", "normalize_allowed_ids", "parse_expiry_ts",
          "ready_payload", "start_payload", "status_payload"}
EXPIRY_FALLBACK_S = 600.0


def ok(value):
    return {"ok": True, "value": value}


def restore():
    for name, fn in ORIG.items():
        setattr(live, name, fn)


def call(fn, *args):
    try:
        return ok(fn(*args))
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


def endpoint(fn, *args):
    try:
        return ok(asyncio.run(fn(*args)))
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}
    finally:
        restore()


def canned(payload):
    async def fake(method, path, *, body=None, bearer_token=None):
        return dict(payload) if isinstance(payload, dict) else {}
    return fake


def parse_expiry(req):
    value = req.get("value")
    now = time.time()
    got = live._parse_expiry_ts(value)
    if abs(got - (now + EXPIRY_FALLBACK_S)) < 120:
        return ok(None)          # the clock fallback, not a parse
    return ok(round(got, 6))


def start_payload(req):
    payload = req.get("payload")
    live._telegram_onboarding_pairings.clear()
    live._telegram_onboarding_request = canned(payload)
    return endpoint(live.start_telegram_onboarding,
                    live.TelegramOnboardingStart(bot_name=None))


def status_payload(req):
    rec = req.get("record") if isinstance(req.get("record"), dict) else {}
    payload = req.get("payload")
    pairing_id = "pair-under-test"
    record = live._TelegramOnboardingPairing(
        poll_token=rec.get("poll_token") or "poll-token",
        expires_at=rec.get("expires_at") or "2026-01-02T03:04:05Z",
        expires_at_ts=time.time() + 600,
        bot_token=rec.get("bot_token"),
        bot_username=rec.get("bot_username"),
        owner_user_id=rec.get("owner_user_id"))
    live._telegram_onboarding_pairings.clear()
    live._telegram_onboarding_pairings[pairing_id] = record
    live._telegram_onboarding_request = canned(payload)
    return endpoint(live.get_telegram_onboarding_status, pairing_id)


def apply_ids(req):
    ids = req.get("ids")
    ids = list(ids) if isinstance(ids, list) else []
    pairing_id = "pair-apply"
    record = live._TelegramOnboardingPairing(
        poll_token="poll-token", expires_at="2026-01-02T03:04:05Z",
        expires_at_ts=time.time() + 600, bot_token="123:abc", bot_username="acme_bot")
    saved = {}
    live._telegram_onboarding_pairings.clear()
    live._telegram_onboarding_pairings[pairing_id] = record
    live._telegram_record_or_404 = lambda _pid: record
    live._profile_scope = lambda profile: contextlib.nullcontext(None)
    live.save_env_value = lambda key, value: saved.__setitem__(key, value)
    live._write_platform_enabled = lambda platform_id, enabled: None
    live._restart_gateway_after = lambda *a, **k: {"restart_started": True}
    try:
        result = endpoint_keep(live.apply_telegram_onboarding, pairing_id,
                               live.TelegramOnboardingApply(allowed_user_ids=ids), None)
    finally:
        live._telegram_onboarding_pairings.clear()
        restore()
    if result.get("ok"):
        joined = saved.get("TELEGRAM_ALLOWED_USERS")
        result["value"] = joined.split(",") if joined else []
    return result


def endpoint_keep(fn, *args):
    """Like endpoint() but the caller restores the seams itself."""
    try:
        return ok(asyncio.run(fn(*args)))
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


def ready_payload(req):
    rec = req.get("record") if isinstance(req.get("record"), dict) else {}
    obj = type("Record", (), {})()
    obj.bot_username = rec.get("bot_username")
    obj.owner_user_id = rec.get("owner_user_id")
    obj.expires_at = rec.get("expires_at")
    return ok(live._telegram_ready_payload(obj))


def expected(req):
    func = req.get("func")
    if func == "normalize_user_id":
        return call(live._normalize_telegram_user_id, req.get("value"))
    if func == "normalize_allowed_ids":
        return apply_ids(req)
    if func == "parse_expiry_ts":
        return parse_expiry(req)
    if func == "ready_payload":
        return ready_payload(req)
    if func == "start_payload":
        return start_payload(req)
    if func == "status_payload":
        return status_payload(req)
    raise AssertionError("unknown func %r" % (func,))


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
        finally:
            restore()
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
