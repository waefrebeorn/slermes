#!/usr/bin/env python3
"""Differential oracle for agent/model_metadata_token_memo.py
(gate w24_modelmetad_ad7f50).

The memo's globals live in the module, so the case drives the LIVE
``remember_message_tokens``/``lookup_message_tokens``/``forget_message_token_memo``
directly: the cache is cleared between cases, the lock is never contended, and
time is supplied by monkeypatching ``_clock``.  Two Python-only facts are
calibrated rather than assumed: a pin's byte cost (a ``__sizeof__`` that makes
``sys.getsizeof`` equal the size the case declares) and the per-object cost
``sys.getsizeof`` adds, measured once on a probe object.
"""
import json
import os
import sys
import time
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_modelmetad_ad7f50"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import model_metadata_token_memo as live  # noqa: E402

WANTED = {"entry_bytes", "oversize", "memo", "forget", "constants"}
_REAL_CLOCK = live._clock


class _Pin:
    """A leaf whose measured cost is exactly ``n`` bytes."""

    __slots__ = ("n",)

    def __init__(self, n):
        self.n = n

    def __sizeof__(self):
        return self.n


# sys.getsizeof reports __sizeof__() plus a fixed per-object cost; measure it.
_PIN_OVERHEAD = sys.getsizeof(_Pin(1000)) - 1000


def pins(sizes):
    return live.MemoPins(_Pin(size - _PIN_OVERHEAD) for size in sizes)


def entry_bytes(req):
    sizes = req.get("pin_sizes")
    sizes = [s for s in sizes if isinstance(s, (int, float)) and not isinstance(s, bool)] if isinstance(sizes, list) else []
    return live._entry_bytes((pins(sizes), 0, 0))


def oversize(req):
    size = req.get("size")
    return isinstance(size, (int, float)) and not isinstance(size, bool) and size > live._MSG_TOKENS_CACHE_MAX_BYTES


def _reset():
    live._MSG_TOKENS_CACHE.clear()
    live._retained_bytes = 0
    live._clock = _REAL_CLOCK


def memo(req):
    _reset()
    ops = req.get("ops")
    for op in (ops if isinstance(ops, list) else [])[:8192]:
        if not isinstance(op, dict):
            continue
        now = op.get("now", 0.0)
        live._clock = (lambda stamp=now: stamp)
        if op.get("op") == "lookup":
            live.lookup_message_tokens(op.get("key"))
        elif op.get("op") == "remember":
            sizes = op.get("sizes")
            sizes = [s for s in sizes if isinstance(s, (int, float))] if isinstance(sizes, list) else []
            live.remember_message_tokens(op.get("key"), (pins(sizes), 0, 0))
    return {"keys": list(live._MSG_TOKENS_CACHE.keys()), "retained": live.retained_bytes()}


def _resolve(value, texts):
    if isinstance(value, str):
        return texts.get(value, value)
    if isinstance(value, dict):
        return {k: _resolve(v, texts) for k, v in value.items()}
    if isinstance(value, list):
        return [_resolve(v, texts) for v in value]
    return value


def forget(req):
    _reset()
    texts = req.get("texts") if isinstance(req.get("texts"), dict) else {}
    memo_rows = req.get("memo")
    for item in memo_rows if isinstance(memo_rows, list) else []:
        if not isinstance(item, dict):
            continue
        names = [n for n in (item.get("pins") or []) if n in texts]
        live.remember_message_tokens(item.get("key"), (live.MemoPins(texts[n] for n in names), 0, 0))
    messages = _resolve(req.get("messages"), texts)
    keep_raw = req.get("keep")
    keep = _resolve(keep_raw, texts) if keep_raw is not None else None
    count = live.forget_message_token_memo(messages, keep=keep)
    return {"evicted": count, "keys": list(live._MSG_TOKENS_CACHE.keys())}


def constants():
    return {
        "max_entries": live._MSG_TOKENS_CACHE_MAX,
        "max_bytes": live._MSG_TOKENS_CACHE_MAX_BYTES,
        "leaf_overhead": live._LEAF_OVERHEAD_BYTES,
        "active_window_s": live._ACTIVE_WINDOW_S,
        "long_string": 64,
    }


def expected(r):
    f = r.get("func")
    if f == "entry_bytes":
        return entry_bytes(r)
    if f == "oversize":
        return oversize(r)
    if f == "memo":
        return memo(r)
    if f == "forget":
        return forget(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def eq(a, b):
    """Python equality: absent key == null == None, True == 1, 1 == 1.0."""
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, (bool, int, float)) and isinstance(b, (bool, int, float)) and a == b
    if isinstance(a, dict) and isinstance(b, dict):
        return set(a) == set(b) and all(eq(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(eq(x, y) for x, y in zip(a, b))
    if a is None or b is None:
        return a is b
    return a == b


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
        got = r.get("out")
        if not eq(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
