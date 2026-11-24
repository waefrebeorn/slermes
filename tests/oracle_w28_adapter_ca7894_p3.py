#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/email/adapter.py (gate w28_adapter_ca7894_p3).

The live adapter module is imported BY FILE PATH: its package __init__ pulls the
gateway stack (fastapi/pydantic), and this host's application-control policy
refuses pydantic's compiled extension.  The seam stubs cover that import chain.

Seams used here (I/O the port deliberately does not own):

  _get_secret          the profile-scoped env reader  -> the fixture "env" object
  _is_sender_authorized  the gateway allowlist/pairing verdict -> the fixture
                       "authorized" tri-state (null = "no verdict")
  hash_for_log         hashed identifiers for logs   -> a constant string
  cache_image_from_bytes / cache_document_from_bytes  the attachment cache writers
                       -> in-memory sentinels
  EmailAdapter.__new__ the instance hole (no IMAP/SMTP socket, no poll task)

Ported symbols and their live entry points:

  verify_sender_auth   _verify_sender_authentication(msg, from_addr, authserv_id=)
  sender_accepted      EmailAdapter._sender_accepted(sender_addr, msg_data)
  trim_seen_uids       EmailAdapter._trim_seen_uids()
  message_id_domain    EmailAdapter._message_id_domain()
  reply_threading      EmailAdapter._new_reply(to_addr, body, reply_to)
  attachment_meta      _extract_attachments(msg, skip_attachments=)
"""
import json
import logging
import os
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_adapter_ca7894_p3"

_TMPHOME = tempfile.mkdtemp(prefix="w28-email-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

import importlib.util  # noqa: E402

_SPEC = importlib.util.spec_from_file_location(
    "w28_email_adapter_live", QUARRY / "plugins" / "platforms" / "email" / "adapter.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

ADAPTER = live.EmailAdapter
WANTED = {"verify_sender_auth", "sender_accepted", "trim_seen_uids",
          "message_id_domain", "reply_threading", "attachment_meta"}

live.hash_for_log = lambda value: "h:seam"


class _Msg:
    """The only Message method _verify_sender_authentication uses."""

    def __init__(self, values):
        self._values = list(values)

    def get_all(self, name, failobj=None):
        return self._values or failobj


class _Cap(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.DEBUG)
        self.levels = []

    def emit(self, record):
        self.levels.append(record.levelno)


def _instance(address="bot@example.com", require_auth=True, extra=None):
    inst = ADAPTER.__new__(ADAPTER)
    inst._address = address
    inst._require_authenticated_sender = require_auth
    inst.config = types.SimpleNamespace(extra=dict(extra or {}))
    return inst


def verify_sender_auth(req):
    m = _Msg(req.get("headers") or [])
    return list(live._verify_sender_authentication(
        m, req.get("from_addr") or "", authserv_id=req.get("authserv_id") or ""))


def sender_accepted(req):
    env = req.get("env") or {}
    extra = req.get("extra") or {}
    msg = req.get("msg") or {}
    authorized = req.get("authorized")
    live._get_secret = lambda name, default="": env.get(name, default)
    inst = _instance(address=req.get("address") or "", require_auth=bool(req.get("require_auth", True)),
                     extra=extra)
    inst._is_sender_authorized = lambda *a, **k: authorized
    cap = _Cap()
    live.logger.addHandler(cap)
    live.logger.setLevel(logging.DEBUG)
    live.logger.propagate = False
    try:
        accepted = inst._sender_accepted(req.get("sender") or "", dict(msg))
    finally:
        live.logger.removeHandler(cap)
    if any(level >= logging.WARNING for level in cap.levels):
        log = "warning"
    elif cap.levels:
        log = "debug"
    else:
        log = ""
    return {"accepted": accepted, "log": log}


def trim_seen_uids(req):
    inst = _instance()
    inst._seen_uids = set(req.get("uids") or [])
    inst._seen_uids_max = int(req.get("max", 2000))
    inst._trim_seen_uids()
    return sorted(inst._seen_uids, key=lambda u: int(u))


def message_id_domain(req):
    return _instance(address=req.get("address") or "")._message_id_domain()


def reply_threading(req):
    ctx = req.get("ctx")
    inst = _instance(address=req.get("address") or "")
    inst._thread_context = {req.get("to_addr") or "": dict(ctx)} if ctx else {}
    msg, _msg_id, subject = inst._new_reply(req.get("to_addr") or "", "body", req.get("reply_to"))
    return {"subject": subject,
            "in_reply_to": msg.get("In-Reply-To"),
            "references": msg.get("References")}


class _Part:
    def __init__(self, d):
        self.d = d

    def get(self, key, default=""):
        return self.d.get(key, default)

    def get_content_type(self):
        return self.d["content_type"]

    def get_content_subtype(self):
        return self.d["subtype"]

    def get_filename(self):
        return self.d.get("filename")

    def get_payload(self, decode=False):
        return b"payload" if self.d.get("has_payload", True) else None


class _Multipart:
    def __init__(self, part):
        self.part = part

    def is_multipart(self):
        return True

    def walk(self):
        return iter([self.part])


def attachment_meta(req):
    live.cache_image_from_bytes = lambda payload, ext: "/cache/image" + ext
    live.cache_document_from_bytes = lambda payload, name: "/cache/document"
    part = _Part({
        "Content-Disposition": req.get("disposition") or "",
        "content_type": req.get("content_type") or "text/plain",
        "subtype": req.get("subtype") or "",
        "filename": req.get("filename"),
        "has_payload": bool(req.get("has_payload", True)),
    })
    out = live._extract_attachments(_Multipart(part), skip_attachments=bool(req.get("skip", False)))
    if not out:
        return None
    first = out[0]
    return {"filename": first["filename"], "type": first["type"],
            "media_type": first["media_type"]}


HANDLERS = {
    "verify_sender_auth": verify_sender_auth,
    "sender_accepted": sender_accepted,
    "trim_seen_uids": trim_seen_uids,
    "message_id_domain": message_id_domain,
    "reply_threading": reply_threading,
    "attachment_meta": attachment_meta,
}


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
            py = HANDLERS[r["func"]](r)
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
