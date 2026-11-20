#!/usr/bin/env python3
"""Generate tests/fixtures/nous_rate_guard_flow2.jsonl for the nrg2 FLOW port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts}}}.
Script semantics (shared with the C harness dispatcher and the oracle Script):
  - absent key            -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M} -> raised exception of kind K
  - {"__script__":true,"ret":X} -> list-valued result X (queue-ambiguity wrap)
  - null                  -> None result
>=5 cases per function (3 functions); covers branches, seam short-circuiting,
max-on-ties, int() truncation, the full builtin exception hierarchy
(Exception/OSError families, BaseException trio, groups, synthetic kinds) and
fail-closed paths. Domain: numeric/bool/null values only; bucket scripts are
dicts of exactly 2-element [remaining, reset] arrays (or null/non-dict for the
AttributeError paths), or raises.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "nous_rate_guard_flow2.jsonl"

CASES = []


def add(func, inp, io=None):
    inp = dict(inp)
    inp["io"] = io or {}
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def L(ret):
    return {"__script__": True, "ret": ret}


def B(rem, rst):
    return [rem, rst]


FUNCS = ["_is_exhausted", "is_long_welcome_rate_limit", "welcome_refusal_from_headers"]

# --------------------------------------------------------------- _is_exhausted
F = "_is_exhausted"
for remaining, reset in [
    (0, 60.0),        # both gates true -> True
    (0, 59.9),        # reset below threshold -> False
    (None, 100.0),    # remaining is None -> False
    (1, 100.0),       # remaining > 0 -> False
    (-5, 60.0),       # negative remaining -> True
    (False, 60.0),    # bool admitted as int: False <= 0 -> True
    (True, 60.0),     # True <= 0 -> False
    (0, True),        # reset True (=1) < 60 -> False
    (0, None),        # reset None -> False
    (0.0, 1e9),       # float remaining, big reset -> True
    (2, 100.0),       # positive remaining -> False
    (0, 60),          # reset exactly at threshold -> True
]:
    add(F, {"remaining": remaining, "reset": reset})

# ----------------------------------------------- is_long_welcome_rate_limit
F = "is_long_welcome_rate_limit"
for ec in [None, "x", 5, [], {}, {"welcome_refusal": "x"},
           {"welcome_refusal": {"reason": "at_capacity"}},
           {"welcome_refusal": {"reason": None}}]:
    add(F, {"error_context": ec})   # no safe_float seam fires
# reason is rate_limited -> safe_float seam fires
OK = {"welcome_refusal": {"reason": "rate_limited", "retry_after": "120s"}}
add(F, {"error_context": OK}, {"safe_float": 25.0})     # >= 20 -> True
add(F, {"error_context": OK}, {"safe_float": 19.9})     # < 20 -> False
add(F, {"error_context": OK}, {"safe_float": 20.0})     # boundary -> True
add(F, {"error_context": OK}, {"safe_float": 0})        # zero -> False
add(F, {"error_context": OK}, {"safe_float": -5})       # negative -> False
add(F, {"error_context": OK}, {"safe_float": None})     # None >= float -> TypeError
add(F, {"error_context": OK}, {"safe_float": R("ValueError", "bad")})
add(F, {"error_context": OK}, {"safe_float": R("SystemExit", "bye")})
add(F, {"error_context": OK}, {"safe_float": R("KeyboardInterrupt", "ki")})
add(F, {"error_context": OK}, {})                       # fail-closed

# ----------------------------------------------- welcome_refusal_from_headers
F = "welcome_refusal_from_headers"
# short-circuit: exhausted_wait truthy -> lower_headers/parse_retry NOT called
add(F, {"headers": {"tier": B(0, 25.0)}},
    {"parse_buckets_from_headers": {"tier": B(0, 25.0)}})
add(F, {"headers": {"tier": B(0, 5.0)}},
    {"parse_buckets_from_headers": {"tier": B(0, 5.0)}})       # empty -> rate_limited
add(F, {"headers": {"a": B(0, 10.0), "b": B(0, 30.0)}},
    {"parse_buckets_from_headers": {"a": B(0, 10.0), "b": B(0, 30.0)}})  # max on ties/values
add(F, {"headers": {"tier": B(0, 20.0)}},
    {"parse_buckets_from_headers": {"tier": B(0, 20.0)}})      # exactly 20 -> rate_limited
add(F, {"headers": {"tier": B(0, 19.999)}},
    {"parse_buckets_from_headers": {"tier": B(0, 19.999)}})    # <20 but empty -> rate_limited
# healthy bucket -> lower_headers + parse_retry_after_seconds fire
HDR = {"h": B(5, 100.0)}
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": {}, "parse_retry_after_seconds": 30})
add(F, {"headers": HDR, "body_wait": 12.5},
    {"parse_buckets_from_headers": HDR, "lower_headers": {}, "parse_retry_after_seconds": None})
add(F, {"headers": HDR, "body_wait": -5.0},
    {"parse_buckets_from_headers": {"h": B(0, None)}, "lower_headers": {},
     "parse_retry_after_seconds": None})                        # empty bucket, negative wait
add(F, {"headers": HDR, "body_wait": True},
    {"parse_buckets_from_headers": HDR, "lower_headers": {}, "parse_retry_after_seconds": None})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR,
     "lower_headers": {"retry-after": "120s"}, "parse_retry_after_seconds": 45})
add(F, {"headers": {}, "body_wait": None},
    {"parse_buckets_from_headers": {}, "lower_headers": {}, "parse_retry_after_seconds": None})
add(F, {"headers": {"tier": B(0, 0)}, "body_wait": None},
    {"parse_buckets_from_headers": {"tier": B(0, 0)}, "lower_headers": {},
     "parse_retry_after_seconds": None})
# exception hierarchy + fail-closed paths
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": R("ValueError", "v")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": R("SystemExit", "bye")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": R("BaseExceptionGroup", "grp")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": None})                       # AttributeError values
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": [1, 2]})                     # AttributeError values (list)
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": None}) # AttributeError get
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": {},
     "parse_retry_after_seconds": R("TypeError", "t")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": {},
     "parse_retry_after_seconds": R("KeyboardInterrupt", "ki")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": {},
     "parse_retry_after_seconds": R("FileNotFoundError", "nf")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": {},
     "parse_retry_after_seconds": R("ZeroDivisionError", "zd")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR, "lower_headers": {},
     "parse_retry_after_seconds": R("CustomSyntheticError", "custom")})
add(F, {"headers": HDR, "body_wait": None},
    {"parse_buckets_from_headers": HDR})                        # fail-closed lower_headers
add(F, {"headers": HDR, "body_wait": None}, {})                 # fail-closed parse_buckets


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %s: %d cases" % (OUT, len(CASES)))
    for fn in FUNCS:
        n = counts.get(fn, 0)
        print("  %-32s %d%s" % (fn, n, "" if n >= 5 else "  (<5!)"))
    assert all(counts.get(fn, 0) >= 5 for fn in FUNCS), "every function needs >=5 cases"
    assert set(counts) == set(FUNCS), "expected exactly the 3 unclaimed functions"


if __name__ == "__main__":
    main()
