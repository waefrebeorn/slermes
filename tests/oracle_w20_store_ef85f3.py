#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/store.py (w20_store_ef85f3).

A real ``TeamsPipelineStore`` is built per case (its file lives in the isolated
SLERMES_HOME the gate hands both processes), so the live module does the work:
``_load`` for a store document, ``build_notification_receipt_key``,
``_upsert`` and ``record_notification_receipt``. The only seam is the clock --
``_utc_now_iso`` -- which the fixture supplies, and the store's own bucket that
each case seeds before the call.
"""
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import plugins.teams_pipeline.store as live  # noqa: E402

GATE = "w20_store_ef85f3"
BUCKETS = ("subscriptions", "notification_receipts", "event_timestamps", "jobs", "sink_records")
UPSERTS = {"subscriptions": "upsert_subscription", "jobs": "upsert_job",
           "sink_records": "upsert_sink_record"}


def tmp_home():
    return Path(tempfile.mkdtemp(prefix="store85_"))


def expected(r):
    f = r.get("func")
    now = r.get("now") or "2026-01-01T00:00:00+00:00"
    live._utc_now_iso = lambda: now
    home = tmp_home()
    try:
        if f == "bucket_name":
            i = int(r.get("index") or 0)
            return live._BUCKETS[i] if 0 <= i < len(live._BUCKETS) else None
        if f == "is_bucket":
            return (r.get("name") or "") in live._BUCKETS
        if f == "load_state":
            text = r.get("text") or ""
            path = home / "store.json"
            path.write_text(text, encoding="utf-8")
            try:
                store = live.TeamsPipelineStore(path)
            except Exception:  # noqa: BLE001 - json.loads / dict() refusing the document
                return {"buckets": {b: {} for b in BUCKETS},
                        "stats": {b: 0 for b in BUCKETS}, "parse_error": True}
            return {"buckets": {b: store._state[b] for b in BUCKETS},
                    "stats": store.stats()}
        if f == "receipt_key":
            return live.TeamsPipelineStore.build_notification_receipt_key(r.get("notification") or {})
        if f == "upsert":
            bucket = r.get("bucket") or "subscriptions"
            store = live.TeamsPipelineStore(home / "store.json")
            if isinstance(r.get("existing"), dict):
                store._state[bucket] = {r.get("key"): dict(r["existing"])}
            method = getattr(store, UPSERTS[bucket])
            return method(r.get("key") or "", r.get("payload") or {})
        if f == "record_receipt":
            store = live.TeamsPipelineStore(home / "store.json")
            if isinstance(r.get("receipts"), dict):
                store._state["notification_receipts"] = dict(r["receipts"])
            added = store.record_notification_receipt(
                r.get("key") or "", r.get("payload"),
                received_at=r.get("received_at") or None)
            return {"added": added, "receipts": store._state["notification_receipts"]}
    finally:
        shutil.rmtree(home, ignore_errors=True)
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
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
