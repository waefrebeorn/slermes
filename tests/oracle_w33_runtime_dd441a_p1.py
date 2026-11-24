#!/usr/bin/env python3
"""Differential oracle for persistence/authority/runtime.py — gate
w33_runtime_dd441a_p1 (receipt identity: the exact-retry key and digest).

The live module is imported as a package module; the store, the authority
epoch and every transaction are seams that never run.  What is compared is
exactly what the values decide:

  * ``mutation_receipt_key`` — the receipt key (``gateway.mutation.v1.`` + the
    canonical identity digest) and the mutation fingerprint digest, both the
    ``admission_fingerprint`` sha256 over ``{"target", "payload"}``
  * ``chat_pick_key`` — the fenced spelling: the bounded identity texts, the
    non-negative int fences, the canonical payload round-trip
  * ``_text`` — the bounded identity text rule
  * ``COMPACTED_TARGET_DETAIL`` — the constant clients match on

Envelope note: a raise is reported as ``{"ok": false, "error": <reason>}``.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_runtime_dd441a_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

sys.path.append(str(Path(__file__).resolve().parent))  # hermes_seam_stubs
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.authority import runtime as live  # noqa: E402
from persistence.core.errors import AuthorityRefusal  # noqa: E402

WANTED = {"mutation_receipt_key", "chat_pick_key", "text", "constants"}


def envelope(fn):
    try:
        return fn()
    except AuthorityRefusal as exc:
        return {"ok": False, "error": exc.reason}


def receipt(req):
    key, digest = live.mutation_receipt_key(
        req.get("principal_id"),
        req.get("session_id"),
        req.get("request_id"),
        operation=req.get("operation"),
        payload=req.get("payload"),
        expected_revision=req.get("expected_revision"),
        expected_generation=req.get("expected_generation"),
    )
    return {"ok": True, "key": key, "digest": digest}


def chat_pick(req):
    params = live.ChatPickParams(
        request_id=req.get("request_id"),
        operation=req.get("operation"),
        payload=req.get("payload"),
        expected_revision=req.get("expected_revision"),
        expected_generation=req.get("expected_generation"),
    )
    key, digest = live.chat_pick_key(req.get("principal_id"), req.get("session_id"), params)
    return {"ok": True, "key": key, "digest": digest}


def text_rule(req):
    value = live._text(req.get("value"))
    return {"ok": True, "value": value, "length": len(value)}


def expected(r):
    f = r.get("func")
    if f == "mutation_receipt_key":
        return envelope(lambda: receipt(r))
    if f == "chat_pick_key":
        return envelope(lambda: chat_pick(r))
    if f == "text":
        return envelope(lambda: text_rule(r))
    if f == "constants":
        return {"compacted_target_detail": live.COMPACTED_TARGET_DETAIL}
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
