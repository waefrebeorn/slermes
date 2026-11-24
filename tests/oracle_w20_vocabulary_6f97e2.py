#!/usr/bin/env python3
"""Differential oracle for persistence/authority/vocabulary.py (w20_vocabulary_6f97e2).

The live module is imported straight from the quarry (its only imports are
stdlib), then each harness line is replayed through the live helpers. Compared
values: the canonical admission fingerprint (json.dumps(sort_keys,
ensure_ascii=False, separators=(",", ":")) + sha256), the shared numeric
constants, ADMITTED_REQUEST_CONFLICT, and the three label sets' membership.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_vocabulary_6f97e2_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "authority", "vocabulary.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "fingerprint":
        text = r.get("payload_json")
        payload = json.loads(text) if isinstance(text, str) and text else None
        return live.admission_fingerprint(canonical_target=r.get("target") or "", payload=payload)
    if f == "queue_max":
        return live.ADMISSION_QUEUE_MAX
    if f == "retry_horizon":
        return live.RETRY_HORIZON_S
    if f == "admitted_request_conflict":
        return dict(live.ADMITTED_REQUEST_CONFLICT)
    if f == "classify":
        label = r.get("label")
        return {
            "legacy_local": label in live.LEGACY_LOCAL_SOURCES,
            "legacy_creation_source": live.LEGACY_CREATION_SOURCES.get(label),
            "not_client": label in live.NOT_CLIENT_LABELS,
        }
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - report the live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_vocabulary_6f97e2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
