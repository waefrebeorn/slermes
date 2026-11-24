#!/usr/bin/env python3
"""Differential oracle for agent/credential_pool_model_cooldowns.py
(gate w20_credential_86c0f9).

The live module is imported from the quarry and its pool-bound mixin methods are
driven with a fake pool object, so the compared values come from the live policy
itself. Only the impure readers are scripted per case: the wall clock
(``live.time``), ``_exhausted_ttl`` / ``_normalize_error_context`` /
``_exhausted_until`` from agent.credential_pool, and the pool's own
``_adopt`` / ``_persist`` / ``_is_sole_credential``.
"""
import importlib.util
import json
import os
import sys
import threading
import types

sys.dont_write_bytecode = True
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import agent.credential_pool as cp                       # noqa: E402
from agent.error_classifier import FailoverReason        # noqa: E402

NAME = "w20_credential_86c0f9_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "credential_pool_model_cooldowns.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

MIXIN = live.CredentialPoolModelCooldownMixin


class Pool:
    """Fake credential pool: records the adopt/persist the policy performs."""

    def __init__(self, entries, sole=False):
        self._lock = threading.Lock()
        self._entries = entries
        self._sole = sole
        self.adopts = []
        self.persisted = 0

    def _adopt(self, scoped, persist=False, model_cooldowns=None):
        self.adopts.append((scoped, persist, model_cooldowns))

    def _persist(self):
        self.persisted += 1

    def _is_sole_credential(self):
        return self._sole


def make_entry(raw):
    raw = raw if isinstance(raw, dict) else {}
    return types.SimpleNamespace(
        id=raw.get("id", ""),
        runtime_api_key=raw.get("runtime_api_key"),
        model_cooldowns=raw.get("model_cooldowns"),
        last_status=raw.get("last_status", "ok"),
        until=raw.get("until"),
    )


def expected(r):
    f = r.get("func")
    now = float(r.get("now", 1731000000.0))
    live.time = types.SimpleNamespace(time=lambda: now,
                                      monotonic=lambda: now,
                                      sleep=lambda _s: None)

    if f == "pool_failure_reason":
        reason = FailoverReason[r["reason"]]
        return live.pool_failure_reason(reason, r.get("error_context"),
                                        bool(r.get("billing_unverified")))
    if f == "model_cooldown_until":
        entry = types.SimpleNamespace(model_cooldowns=r.get("cooldowns"))
        return live.model_cooldown_until(entry, r.get("model"))
    if f == "merge_model_cooldowns":
        maps = r.get("maps")
        return live.merge_model_cooldowns(*maps) if isinstance(maps, list) else live.merge_model_cooldowns()
    if f == "is_model_scoped_failure":
        pool = types.SimpleNamespace(provider=r.get("provider"))
        return MIXIN._is_model_scoped_failure(pool, r.get("status_code"), r.get("model"),
                                              r.get("failure_reason"))
    if f == "token_is_blocked":
        entries = [make_entry(e) for e in (r.get("entries") or [])]
        pool = Pool(entries)
        return MIXIN.token_is_blocked(pool, r.get("token"), model=r.get("model"))
    if f == "cool_down_model":
        entries = [make_entry(e) for e in (r.get("entries") or [])]
        pool = Pool(entries, bool(r.get("sole_credential")))
        ttl = float(r.get("exhausted_ttl", 0.0))
        reset_at = r.get("reset_at")

        cp._exhausted_ttl = lambda code, sole_credential=False, failure_reason=None: ttl
        cp._normalize_error_context = (
            lambda ctx: {"reset_at": ctx.get("reset_at")} if isinstance(ctx, dict) else {})
        index = int(r.get("entry_index", 0))
        entry = entries[index]
        MIXIN._cool_down_model(pool, entry, r.get("model"), {"reset_at": reset_at},
                               r.get("failure_reason"))
        order = {id(e): i for i, e in enumerate(entries)}
        return {
            "adopts": [{"index": order[id(scoped)], "persist": bool(persist),
                        "model_cooldowns": cooldowns}
                       for scoped, persist, cooldowns in pool.adopts],
            "persisted": bool(pool.persisted),
        }
    if f == "limit_state":
        entries = [make_entry(e) for e in (r.get("entries") or [])]
        pool = Pool(entries)
        cp._exhausted_until = lambda entry, sole_credential=False: entry.until
        return MIXIN.limit_state(pool, r.get("models") or [])
    if f == "entitlement_seconds":
        return live.MODEL_ENTITLEMENT_BENCH_SECONDS
    if f == "usage_window":
        return live.FAILURE_REASON_USAGE_WINDOW
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
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_credential_86c0f9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
