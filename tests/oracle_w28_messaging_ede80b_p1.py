#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/messaging.py — gate w28_messaging_ede80b_p1.

WhatsApp QR onboarding.  The live module is imported BY FILE PATH (its package
__init__ drags the whole web stack) and only its pure value-shaping helpers and
the pairing-event state machine are called:

  * ``_normalize_whatsapp_onboarding_mode`` / ``_normalize_whatsapp_allowed_users``
  * ``_whatsapp_phone_from_identifier`` / ``_first_str``
  * ``_whatsapp_linked_account_from_session`` (given a thrown-away creds.json --
    the file read is the seam, the parse is the logic)
  * ``_apply_pairing_event`` over a bare record object (no lock, no subprocess)

The C side returns an envelope: {"ok": true, "value": ...} or
{"ok": false, "status": ..., "detail": ...}, which is what an HTTPException
becomes here.
"""
import importlib.util
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_messaging_ede80b_p1"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "messaging.py"

# hermes_constants resolves Path.home() at import time; the harness shell hands
# Python an MSYS $HOME, so give it the native profile directory.
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

WANTED = {
    "normalize_mode", "normalize_allowed_users", "phone_from_identifier",
    "first_str", "linked_account", "apply_pairing_event",
}
RECORD_FIELDS = ("qr_payload", "status", "error", "account_id", "account_name", "account_phone")


def ok(value):
    return {"ok": True, "value": value}


def call(fn, *args):
    try:
        return ok(fn(*args))
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


class Record:
    """Stand-in for _WhatsAppOnboardingSession: only the fields the machine reads."""


def apply_event(req):
    rec = Record()
    src = req.get("record") if isinstance(req.get("record"), dict) else {}
    for field in RECORD_FIELDS:
        setattr(rec, field, src.get(field))
    payload = req.get("payload") if isinstance(req.get("payload"), dict) else {}
    live._apply_pairing_event(rec, payload)
    return ok({field: getattr(rec, field) for field in RECORD_FIELDS})


def linked_account(req):
    payload = req.get("payload")
    tmp = tempfile.mkdtemp(prefix="w28p1-")
    try:
        (Path(tmp) / "creds.json").write_text(json.dumps(payload), encoding="utf-8")
        account_id, account_name, account_phone = live._whatsapp_linked_account_from_session(Path(tmp))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok({"account_id": account_id, "account_name": account_name,
               "account_phone": account_phone})


def expected(req):
    func = req.get("func")
    if func == "normalize_mode":
        return call(live._normalize_whatsapp_onboarding_mode, req.get("value"))
    if func == "normalize_allowed_users":
        return call(live._normalize_whatsapp_allowed_users, req.get("value"))
    if func == "phone_from_identifier":
        return call(live._whatsapp_phone_from_identifier, req.get("value"))
    if func == "first_str":
        keys = req.get("keys")
        return call(live._first_str, req.get("candidate"),
                    tuple(keys) if isinstance(keys, list) else ())
    if func == "linked_account":
        return linked_account(req)
    if func == "apply_pairing_event":
        return apply_event(req)
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
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
