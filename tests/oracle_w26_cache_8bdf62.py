#!/usr/bin/env python3
"""Differential oracle for agent/secret_sources/_cache.py (gate w26_cache_8bdf62).

The live module is imported directly.  Everything it decides from values is
compared against the C port:

  * ``fingerprint`` -- the SHA-256 key prefix (a unicode string is encoded UTF-8);
  * ``CachedFetch.is_fresh`` -- the TTL test, with the clock pinned to the
    fixture's ``now``;
  * ``entry_from_payload`` -- the payload -> entry reader that keeps only str
    -> str pairs;
  * ``DiskCache.read`` -- driven through a real cache file written into a
    throw-away home, so the open/parse/mismatch/stale path is the live one;
  * ``DiskCache.write`` -- the atomic write is captured instead of performed,
    so the compared value is exactly the body the disk would receive;
  * ``SecretCache.lookup`` -- L1 over a fixed L2 answer.

The file open, the atomic write, the clock and the home directory are the
seams: each one is a fixed input from the fixture line.
"""
import json
import sys
import tempfile
import time
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_cache_8bdf62"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.secret_sources import _cache as live  # noqa: E402

WANTED = {"fingerprint", "is_fresh", "entry_from_payload", "disk_read",
          "write_payload", "lookup"}
BASENAME = "oracle_secret_cache.json"


def as_dict(entry):
    if entry is None:
        return None
    return {"secrets": dict(entry.secrets), "fetched_at": entry.fetched_at}


def make_entry(raw):
    if not isinstance(raw, dict):
        return None
    return live.CachedFetch(secrets=dict(raw.get("secrets") or {}), fetched_at=raw.get("fetched_at"))


class clock:
    """Pin time.time() to the fixture's ``now`` -- the one clock seam."""

    def __init__(self, now):
        self.now = now

    def __enter__(self):
        self.real = time.time
        time.time = lambda: self.now
        return self

    def __exit__(self, *exc):
        time.time = self.real
        return False


def fingerprint(r):
    return live.fingerprint(r.get("material"))


def is_fresh(r):
    entry = live.CachedFetch(secrets={}, fetched_at=r.get("fetched_at"))
    with clock(r.get("now")):
        return entry.is_fresh(r.get("ttl_seconds"))


def entry_from_payload(r):
    return as_dict(live.entry_from_payload(r.get("payload")))


def disk_read(r):
    payload = r.get("payload")
    with tempfile.TemporaryDirectory() as home, clock(r.get("now")):
        if payload is not None:
            Path(home, BASENAME).write_text(json.dumps(payload), encoding="utf-8")
        cache = live.DiskCache(BASENAME, key_serializer=str)
        return as_dict(cache.read(r.get("want_key"), r.get("ttl_seconds"), Path(home)))


def write_payload(r):
    captured = {}

    def fake_atomic_write(path, payload):
        captured["payload"] = payload

    real = live.atomic_write_json
    live.atomic_write_json = fake_atomic_write
    try:
        with tempfile.TemporaryDirectory() as home:
            cache = live.DiskCache(BASENAME, key_serializer=str)
            entry = make_entry(r.get("entry"))
            cache.write(r.get("key"), entry, r.get("ttl_seconds"), Path(home))
    finally:
        live.atomic_write_json = real
    if "payload" not in captured:
        return {"written": False, "payload": None}
    return {"written": True, "payload": captured["payload"]}


def lookup(r):
    memory = r.get("memory")
    memory = memory if isinstance(memory, dict) else {}
    l2 = make_entry(r.get("disk"))
    cache = live.SecretCache(BASENAME, key_serializer=str)
    objs = {k: make_entry(v) for k, v in memory.items() if isinstance(v, dict)}
    cache.memory = dict(objs)
    with clock(r.get("now")):
        out = cache.lookup(r.get("key"), r.get("ttl_seconds"), None, read_disk=lambda: l2)
    if out is None:
        return {"source": None, "entry": None}
    source = "memory" if objs.get(r.get("key")) is out else "disk"
    return {"source": source, "entry": as_dict(out)}


def expected(r):
    f = r.get("func")
    if f == "fingerprint":
        return fingerprint(r)
    if f == "is_fresh":
        return is_fresh(r)
    if f == "entry_from_payload":
        return entry_from_payload(r)
    if f == "disk_read":
        return disk_read(r)
    if f == "write_payload":
        return write_payload(r)
    if f == "lookup":
        return lookup(r)
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
