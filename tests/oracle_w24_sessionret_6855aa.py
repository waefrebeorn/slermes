#!/usr/bin/env python3
"""Differential oracle for gateway/session_retry_admission.py
(gate w24_sessionret_6855aa).

The live ``RetryRewrite`` and ``take`` are called directly.  The ContextVar is
real: each case binds its rewrite with ``live.bind`` and unbinds afterwards, so
``_bound.get()`` returns exactly what the C side was handed (a null binding is
bound as None, the ContextVar's own default).

Object identity (``is``) is carried by an identity token: the oracle builds one
Python object per token and memoises it, so "same token" means "same object" --
the same rule the C side implements.  The store, the admission write and the
on_commit callback stay seams; nothing is committed, drained or rewritten.
"""
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_sessionret_6855aa"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway import session_retry_admission as live  # noqa: E402

WANTED = {"take", "rewrite", "identity"}

MAX_CALLS = 8


class Tok:
    """One object identity, named by its token."""

    def __init__(self, token):
        self.t = token

    def __repr__(self):
        return "Tok(%r)" % (self.t,)


def objects():
    memo = {}

    def obj(token):
        if token is None:
            return None
        if token not in memo:
            memo[token] = Tok(token)
        return memo[token]

    def tok(value):
        return None if value is None else value.t

    return obj, tok


def take(r):
    obj, tok = objects()
    raw = r.get("bound")
    if isinstance(raw, dict):
        rewrite = live.RetryRewrite(
            store=obj(raw.get("store")),
            rewrite=None,
            source=obj(raw.get("source")),
            on_commit=lambda: None,
            applied=bool(raw.get("applied")),
            taken=bool(raw.get("taken")),
        )
    else:
        rewrite = None
    event = types.SimpleNamespace(source=obj(r.get("event_source")))
    store = obj(r.get("store"))
    calls = r.get("calls", 1)
    calls = int(calls) if isinstance(calls, (int, float)) and not isinstance(calls, bool) else 1
    calls = max(0, min(calls, MAX_CALLS))

    token = live.bind(rewrite)
    out = []
    try:
        for _ in range(calls):
            got = live.take(event, store)
            out.append(None if got is None else {
                "store": tok(got.store),
                "source": tok(got.source),
                "taken": got.taken,
                "applied": got.applied,
            })
    finally:
        live.unbind(token)
    return out


def rewrite(r):
    obj, tok = objects()
    made = live.RetryRewrite(store=obj(r.get("store")), rewrite=None,
                             source=obj(r.get("source")), on_commit=lambda: None)
    return {"store": tok(made.store), "source": tok(made.source),
            "taken": made.taken, "applied": made.applied}


def identity(r):
    obj, _tok = objects()
    return obj(r.get("a")) is obj(r.get("b"))


def expected(r):
    f = r.get("func")
    if f == "take":
        return take(r)
    if f == "rewrite":
        return rewrite(r)
    if f == "identity":
        return identity(r)
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
