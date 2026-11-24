#!/usr/bin/env python3
"""Differential oracle for agent/credential_pool_selection.py (w20_credential_af0c5f).

Imports the live mixin out of the quarry and drives it with a fake pool that
supplies exactly the seams the real CredentialPool supplies: `_available_entries`,
`_entries`, `_strategy`, `_current_unlocked`, `_adopt`, `_persist`, `_find`,
`_log_no_available_entries`.  `random.choice` is pinned to the fixture's index so
the random strategy is deterministic.  Endpoint matching goes through the real
`credential_pool_entry_serves_endpoint` and `normalize_route_base_url`.
"""
import contextlib
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_credential_af0c5f_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "credential_pool_selection.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def entry_objects(raw):
    out = []
    for e in raw or []:
        if not isinstance(e, dict):
            continue
        base = {"request_count": 0, "priority": 0}
        base.update(e)
        out.append(types.SimpleNamespace(**base))
    return out


class FakePool(live.CredentialPoolSelectionMixin):
    def __init__(self, entries, strategy="fill_first", available=None, pending=None,
                 current=None):
        self._entries = list(entries)
        self._strategy = strategy
        self._available = list(available if available is not None else entries)
        self._pending = list(pending or [])
        self._current = current
        self._current_id = current.id if current is not None else None
        self._last_no_entries_log_at = "sentinel"
        self.logged = 0
        self.persisted = 0
        self._lock = contextlib.nullcontext()

    def _available_entries(self, *, clear_expired=False, refresh=True, model=None):
        return list(self._available), list(self._pending)

    def _current_unlocked(self):
        return self._current

    def _adopt(self, entry, persist=False, request_count=None):
        clone = types.SimpleNamespace(**vars(entry))
        if request_count is not None:
            clone.request_count = request_count
        return clone

    def _persist(self):
        self.persisted += 1

    def _find(self, pred):
        return next((e for e in self._entries if pred(e)), None)

    def _log_no_available_entries(self):
        self.logged += 1


def triple(e):
    if e is None:
        return None
    return {"id": getattr(e, "id", None),
            "priority": getattr(e, "priority", 0),
            "request_count": getattr(e, "request_count", 0)}


def endpoint(r):
    url = r.get("endpoint_url")
    return url if isinstance(url, str) and url else None


def pin_random(index):
    live.random = types.SimpleNamespace(
        choice=lambda seq: seq[index % len(seq)] if seq else None)


def replace(obj, **changes):
    """dataclasses.replace() over the fixture's namespace entries."""
    data = dict(vars(obj))
    data.update(changes)
    return types.SimpleNamespace(**data)


live.replace = replace


def expected(r):
    f = r.get("func")
    entries = entry_objects(r.get("entries"))
    if f == "serving":
        return [e.id for e in live._serving(entries, endpoint(r))]
    if f == "strategy_pick":
        pin_random(int(r.get("rng_index") or 0))
        pool = FakePool(entries, strategy=r.get("strategy") or "fill_first")
        return triple(pool._strategy_pick(list(entries)))
    if f == "record_pick":
        pin_random(int(r.get("rng_index") or 0))
        pool = FakePool(entries, strategy=r.get("strategy") or "fill_first",
                        available=entries)
        picked = next((e for e in entries if e.id == r.get("picked_id")), None)
        final = pool._record_pick(picked, list(entries), count=bool(r.get("count", True)))
        return {"entry": triple(final),
                "entries": [triple(e) for e in pool._entries],
                "persisted": bool(pool.persisted)}
    if f == "select_unlocked":
        pin_random(int(r.get("rng_index") or 0))
        available = entry_objects(r.get("available"))
        pool = FakePool(entries, strategy=r.get("strategy") or "fill_first",
                        available=available, pending=[str(x) for x in (r.get("pending") or [])])
        picked, pending = pool._select_unlocked(refresh=True,
                                               count=bool(r.get("count", True)),
                                               model=None, endpoint_url=endpoint(r))
        return {"entry": triple(picked),
                "pending": list(pending),
                "log_no_entries": bool(pool.logged),
                "throttle_cleared": pool._last_no_entries_log_at is None,
                "persisted": bool(pool.persisted)}
    if f == "peek":
        current = entry_objects([r.get("current")]) if isinstance(r.get("current"), dict) else []
        pool = FakePool(entries, available=entry_objects(r.get("available")),
                        current=current[0] if current else None)
        return triple(pool.peek(model=r.get("model"), endpoint_url=endpoint(r)))
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
print("ORACLE w20_credential_af0c5f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
