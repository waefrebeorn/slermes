#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/google_chat/adapter.py (gate
w27_adapter_97bbcb_p2).

Gate 2 of 4: predicates / validation.  Every verdict comes from the live
function:

  * ``_is_google_owned_host`` - the attachment SSRF allow-list;
  * ``_redact_sensitive`` - the Pub/Sub + service-account redactions;
  * ``_http_status`` / ``_is_retryable_error`` - driven with a stand-in
    exception carrying ``resp.status``;
  * ``GoogleChatAdapter._validate_config`` - driven on an instance made with
    ``__new__`` (only ``config.extra`` is consulted);
  * module ``_validate_config`` / ``_is_connected``;
  * ``_GCHAT_CHAT_ID_RE`` / ``_GCHAT_THREAD_ID_RE`` matches;
  * ``verify_http_event_request`` - the id-token verify seam is fed the
    fixture's claims (or made to raise);
  * ``_int_setting`` / ``_str_setting`` - the scoped-secret read is fed the
    fixture's env value.
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
GATE = "w27_adapter_97bbcb_p2"

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.google_chat import adapter as live  # noqa: E402

A = live.GoogleChatAdapter

WANTED = {"is_google_owned_host", "redact_sensitive", "retry_class", "validate_config",
          "plugin_validate_config", "is_connected", "space_id_ok", "thread_id_ok",
          "verify_http_event", "int_setting", "str_setting"}


class Quiet:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def error(self, *a, **k):
        pass


live.logger = Quiet()


def same(py, c):
    if isinstance(py, bool) or isinstance(c, bool):
        return isinstance(py, bool) and isinstance(c, bool) and py == c
    if isinstance(py, float) or isinstance(c, float):
        if not isinstance(py, (int, float)) or not isinstance(c, (int, float)):
            return False
        return abs(float(py) - float(c)) <= 1e-9 * max(1.0, abs(float(py)), abs(float(c)))
    if isinstance(py, dict) and isinstance(c, dict):
        if set(py) != set(c):
            return False
        return all(same(py[k], c[k]) for k in py)
    if isinstance(py, list) and isinstance(c, list):
        return len(py) == len(c) and all(same(x, y) for x, y in zip(py, c))
    return py == c


class Exc(Exception):
    """Stand-in for a googleapiclient HttpError: ``resp.status`` or nothing."""

    def __init__(self, status, message):
        super().__init__(message)
        self.resp = SimpleNamespace(status=status) if status is not None else None


def retry_class(req):
    exc = Exc(req.get("status"), req.get("message") or "")
    return {"status": live._http_status(exc), "retryable": live._is_retryable_error(exc)}


def validate_config(req):
    fake = A.__new__(A)
    fake.config = SimpleNamespace(extra=req.get("extra") or {})
    try:
        project_id, subscription = fake._validate_config()
    except ValueError as exc:
        return {"ok": False, "error": str(exc)}
    return {"ok": True, "project_id": project_id, "subscription": subscription}


def verify_http_event(req):
    fake = A.__new__(A)
    fake._http_events_audience = req.get("audience") or ""
    fake._http_events_service_account_email = req.get("sa_emails") or ""
    claims = req.get("claims")
    raises = bool(req.get("claims_error"))
    orig = live._verify_google_id_token

    def fake_verify(token, audience):
        if raises:
            raise ValueError("invalid token signature")
        if claims is None:
            raise ValueError("no claims")
        return claims

    live._verify_google_id_token = fake_verify
    try:
        ok, reason = fake.verify_http_event_request(req.get("auth_header") or "")
    finally:
        live._verify_google_id_token = orig
    return {"ok": ok, "reason": reason}


def int_setting(req):
    orig = live._get_scoped_secret
    env_value = req.get("env_value") or ""
    live._get_scoped_secret = lambda name, default="": env_value
    try:
        return A._int_setting(req.get("extra") or {}, req.get("key") or "", "GC_ENV", req.get("default") or 0)
    finally:
        live._get_scoped_secret = orig


def str_setting(req):
    orig = live._get_scoped_secret
    env_value = req.get("env_value") or ""
    live._get_scoped_secret = lambda name, default="": env_value
    try:
        return A._str_setting(req.get("extra") or {}, req.get("key") or "", "GC_ENV", req.get("fallback") or "")
    finally:
        live._get_scoped_secret = orig


def expected(r):
    f = r.get("func")
    if f == "is_google_owned_host":
        return live._is_google_owned_host(r.get("url") or "")
    if f == "redact_sensitive":
        return live._redact_sensitive(r.get("text") or "")
    if f == "retry_class":
        return retry_class(r)
    if f == "validate_config":
        return validate_config(r)
    if f == "plugin_validate_config":
        return bool(live._validate_config(SimpleNamespace(extra=r.get("extra") or {})))
    if f == "is_connected":
        cfg = SimpleNamespace(enabled=bool(r.get("enabled")), extra=r.get("extra") or {})
        return bool(live._is_connected(cfg))
    if f == "space_id_ok":
        return bool(live._GCHAT_CHAT_ID_RE.match(r.get("chat_id") or ""))
    if f == "thread_id_ok":
        return bool(live._GCHAT_THREAD_ID_RE.match(r.get("thread_id") or ""))
    if f == "verify_http_event":
        return verify_http_event(r)
    if f == "int_setting":
        return int_setting(r)
    if f == "str_setting":
        return str_setting(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        if not same(py, r.get("out")):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, r.get("out")), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
