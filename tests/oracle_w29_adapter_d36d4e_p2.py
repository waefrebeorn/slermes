#!/usr/bin/env python3
"""Differential oracle for the inbound half of plugins/platforms/signal/adapter.py
(gate w29_adapter_d36d4e_p2).

The live ``SignalAdapter`` is imported BY FILE PATH and instances are made with
``__new__`` so no client, SSE task or lock is created.  What is exercised is the
envelope decision surface:

  * ``_unwrap_sync_message``           Note-to-Self / group sync-sent promotion
  * ``_apply_group_mention_rules``     require_mention gate + own-@ strip
  * ``_group_allowed``                 SIGNAL_GROUP_ALLOWED_USERS policy
  * ``_remember_recipient_identifiers`` number↔UUID cache
  * ``_extract_quote_author`` / ``_quote_references_own_message``
  * ``_remember_sent_message_timestamp`` / ``_consume_sent_timestamp``
  * ``_extract_contact_uuid``
  * ``_handle_envelope``               the full decision chain (handle_message,
    attachment fetch and authorization are stubbed seams: the fixture says
    which media types a successful fetch produced)

Fixture clocks/timestamps are passed in; nothing sleeps and nothing dials out.
"""
import asyncio
import hashlib
import importlib.util
import json
import os
import sys
from collections import OrderedDict
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_d36d4e_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

_spec = importlib.util.spec_from_file_location(
    "sig_adapter_p2", QUARRY / "plugins" / "platforms" / "signal" / "adapter.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)
A = live.SignalAdapter

WANTED = {
    "unwrap_sync_message", "apply_group_mention_rules", "group_allowed",
    "remember_recipient_identifiers", "extract_quote_author",
    "quote_references_own_message", "remember_sent_message_timestamp",
    "extract_contact_uuid", "consume_sent_timestamp", "envelope_decision",
}


def num_keys(mapping):
    """JSON object keys are strings; Python's timestamp dicts key on ints."""
    out = OrderedDict()
    for k, v in (mapping or {}).items():
        try:
            out[int(k)] = v
        except (TypeError, ValueError):
            out[k] = v
    return out


def bare(account="", allow=(), stories=True, mention=False, ignore_attachments=True,
         recent=None, sent_cache=None, max_sent=500, uuid_by_number=None,
         number_by_uuid=None):
    ad = A.__new__(A)
    ad.http_url = "http://127.0.0.1:8080"
    ad.account = account
    ad._account_normalized = (account or "").strip()
    ad.ignore_stories = stories
    ad.require_mention = mention
    ad.group_allow_from = set(allow or ())
    ad.dm_allow_from = {"*"}
    ad.gateway_runner = None
    ad.ignore_attachments = ignore_attachments
    ad._recent_sent_timestamps = num_keys(recent)
    ad._sent_message_timestamps = OrderedDict((str(k), None) for k in (sent_cache or ()))
    ad._max_sent_message_timestamps = max_sent
    ad._recipient_uuid_by_number = dict(uuid_by_number or {})
    ad._recipient_number_by_uuid = dict(number_by_uuid or {})
    return ad


def unwrap_sync_message(r):
    ad = bare(account=r.get("account") or "", recent=r.get("recent_sent"))
    return {"envelope": ad._unwrap_sync_message(r.get("envelope_data") or {})}


def apply_group_mention_rules(r):
    ad = bare(account=r.get("account") or "", mention=bool(r.get("require_mention")),
              uuid_by_number=r.get("uuid_by_number"))
    mentioned, text = ad._apply_group_mention_rules(r.get("text") or "", r.get("data_message") or {})
    return {"mentioned": mentioned, "text": text}


def group_allowed(r):
    ad = bare(account=r.get("account") or "", allow=r.get("allow"))
    return ad._group_allowed(r.get("group_id"))


def remember_recipient_identifiers(r):
    maps = r.get("maps") or {}
    ad = bare(account=r.get("account") or "",
              uuid_by_number=maps.get("number_to_uuid"), number_by_uuid=maps.get("uuid_to_number"))
    ad._remember_recipient_identifiers(r.get("number"), r.get("service_id"))
    return {"number_to_uuid": dict(ad._recipient_uuid_by_number),
            "uuid_to_number": dict(ad._recipient_number_by_uuid)}


def extract_quote_author(r):
    return A._extract_quote_author(r.get("quote"))


def quote_references_own_message(r):
    ad = bare(account=r.get("account") or "", sent_cache=r.get("sent_timestamps"),
              uuid_by_number=r.get("uuid_by_number"), number_by_uuid=r.get("number_by_uuid"))
    return ad._quote_references_own_message(r.get("reply_to_id"), r.get("reply_to_author"))


def remember_sent_message_timestamp(r):
    ad = bare(account=r.get("account") or "", sent_cache=r.get("cache"),
              max_sent=r.get("max", 500))
    ad._remember_sent_message_timestamp(r.get("timestamp"))
    return list(ad._sent_message_timestamps)


def extract_contact_uuid(r):
    ad = bare(account=r.get("account") or "")
    return ad._extract_contact_uuid(r.get("contact"), r.get("phone"))


def consume_sent_timestamp(r):
    ad = bare(account=r.get("account") or "", recent=r.get("recent"))
    echo = ad._consume_sent_timestamp(r.get("ts"))
    return {"echo": bool(echo), "recent": [str(k) for k in ad._recent_sent_timestamps]}


def envelope_decision(r):
    media = r.get("media_types")
    ad = bare(account=r.get("account") or "", allow=r.get("allow"),
              stories=r.get("ignore_stories", True), mention=bool(r.get("require_mention")),
              ignore_attachments=r.get("ignore_attachments", True),
              recent=r.get("recent_sent"), sent_cache=r.get("sent_cache"),
              max_sent=r.get("max_sent", 500), uuid_by_number=r.get("uuid_by_number"),
              number_by_uuid=r.get("number_by_uuid"))
    captured = []

    async def _handle_message(event):
        captured.append(event)

    async def _collect(attachments):
        return (["/cache/att-0"], list(media))

    ad.handle_message = _handle_message
    ad._is_sender_authorized = lambda *a, **k: True
    if media is not None:
        ad._collect_attachments = _collect
    asyncio.run(ad._handle_envelope(r.get("envelope") or {}))
    if not captured:
        return {"dropped": True}
    ev = captured[0]
    return {
        "dropped": False,
        "chat_id": ev.source.chat_id,
        "text": ev.text,
        "msg_type": ev.message_type.value,
        "reply_to_id": ev.reply_to_message_id,
        "reply_to_author": ev.reply_to_author_id,
        "reply_to_name": ev.reply_to_author_name,
        "own": ev.reply_to_is_own_message,
    }


DISPATCH = {
    "unwrap_sync_message": unwrap_sync_message,
    "apply_group_mention_rules": apply_group_mention_rules,
    "group_allowed": group_allowed,
    "remember_recipient_identifiers": remember_recipient_identifiers,
    "extract_quote_author": extract_quote_author,
    "quote_references_own_message": quote_references_own_message,
    "remember_sent_message_timestamp": remember_sent_message_timestamp,
    "extract_contact_uuid": extract_contact_uuid,
    "consume_sent_timestamp": consume_sent_timestamp,
    "envelope_decision": envelope_decision,
}


def digest(v):
    return hashlib.sha1(json.dumps(v, sort_keys=True).encode("utf-8")).hexdigest()[:10]


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            py = {"error": type(exc).__name__}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r/%s c=%r/%s"
                  % (r.get("func"), r.get("case"), py, digest(py), got, digest(got)),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
