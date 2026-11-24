#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_room_attachments.py (gate
w23_hostedroom_e2fcc7).

The module is the checking layer in front of the ``hosted_room_attachments``
storage repository.  The repository itself (rows, canonical bytes in the private
directory beside the room file, the prune/commit transactions) and the wall
clock are seams -- a real run would write a database -- so the live functions
exercised here are exactly the ones that decide what the store is handed:

  * ``_identifier`` -- strip + bounded identifier rule, with its own message text;
  * ``_sniff_known_mime`` -- the magic-byte table;
  * ``_validate_mime_and_kind`` -- kind/MIME agreement for image, pdf, text and
    opaque container bytes;
  * ``decode_content_base64`` / ``encode_content_base64`` -- the bounded, strict
    RPC codec (no data URLs, no unpadded input, per-file limit).

Upload bytes arrive in the fixture as base64 -- that is the transport for raw
bytes in a JSON line, not the subject of the gate.
"""
import base64
import binascii
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "hosted_room_attachments.py"
GATE = "w23_hostedroom_e2fcc7"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("hosted_room_attachments_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["hosted_room_attachments_live"] = live
_spec.loader.exec_module(live)

WANTED = {"identifier", "sniff_mime", "validate_mime_kind", "decode_b64", "encode_b64",
          "consts"}


def raw(r):
    text = r.get("data_b64") or ""
    return base64.b64decode(text) if text else b""


def ok(value):
    return {"ok": True, "value": value}


def err(exc):
    return {"ok": False, "error": str(exc)}


def expected(r):
    f = r.get("func")
    if f == "consts":
        return {
            "max_identifier_chars": live.MAX_IDENTIFIER_CHARS,
            "max_attachment_bytes": live.MAX_ATTACHMENT_BYTES,
            "max_attachments_per_message": live.MAX_ATTACHMENTS_PER_MESSAGE,
            "uncommitted_ttl_seconds": live.UNCOMMITTED_TTL_SECONDS,
            "disbanded_grace_seconds": live.DISBANDED_GRACE_SECONDS,
            "classic_attachment_ttl_seconds": live.CLASSIC_ATTACHMENT_TTL_SECONDS,
        }
    if f == "identifier":
        try:
            return ok(live._identifier(r.get("value"), label=r.get("label", "room_id")))
        except live.AttachmentError as exc:
            return err(exc)
    if f == "sniff_mime":
        return ok(live._sniff_known_mime(raw(r)))
    if f == "validate_mime_kind":
        try:
            live._validate_mime_and_kind(raw(r), kind=r.get("kind"), mime=r.get("mime"))
            return ok(None)
        except live.AttachmentError as exc:
            return err(exc)
    if f == "decode_b64":
        try:
            data = live.decode_content_base64(r.get("value"))
        except live.AttachmentError as exc:
            return err(exc)
        return ok(base64.b64encode(data).decode("ascii"))
    if f == "encode_b64":
        return ok(live.encode_content_base64(raw(r)))
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
