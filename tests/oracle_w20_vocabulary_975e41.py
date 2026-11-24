#!/usr/bin/env python3
"""Differential oracle for persistence/workers/vocabulary.py (w20_vocabulary_975e41).

The live module is imported straight from the quarry (it has no runtime
imports), then every harness line is replayed through its real tables and its
real ``row_annotations``. Compared values: the four frozensets and the
annotation tuple (sorted where the module sorts), membership, the module
constants (64-bit ints as decimal strings) and the annotation shaper.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
NAME = "w20_vocabulary_975e41_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "persistence", "workers", "vocabulary.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

SETS = {
    "worker_message_fields": live.WORKER_MESSAGE_FIELDS,
    "handoff_message_fields": live.HANDOFF_MESSAGE_FIELDS,
    "sidecar_keys": live.SIDECAR_KEYS,
    "constructor_config_keys": live.WORKER_CONSTRUCTOR_CONFIG_KEYS,
    "row_annotation_keys": live.ROW_ANNOTATION_KEYS,
}


def expected(r):
    f = r.get("func")
    if f == "fields":
        which = r.get("which")
        if which not in SETS:
            return None
        if which == "row_annotation_keys":
            return list(live.ROW_ANNOTATION_KEYS)
        return sorted(SETS[which])
    if f == "contains":
        which = r.get("which")
        if which not in SETS:
            return False
        return r.get("key") in SETS[which]
    if f == "constants":
        return {
            "child_separator": live.CHILD_SEPARATOR,
            "receipt_payload_max_bytes": str(live.RECEIPT_PAYLOAD_MAX_BYTES),
            "transcript_batch_max": str(live.TRANSCRIPT_BATCH_MAX),
            "row_annotation_keys": list(live.ROW_ANNOTATION_KEYS),
            "all": list(live.__all__),
        }
    if f == "row_annotations":
        messages = r.get("messages")
        if not isinstance(messages, list):
            messages = []
        return live.row_annotations(messages)
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
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_vocabulary_975e41: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
