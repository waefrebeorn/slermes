#!/usr/bin/env python3
"""
sta_oracle_nous_exhausted.py — Oracle for
agent/nous_rate_guard.py:_has_exhausted_bucket_in_object.

Reads C harness JSON lines (case/ret) from stdin, rebuilds equivalent
SimpleNamespace states, replays LIVE Python, and compares the bools.
"""
import sys
import os
import json
import types

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.nous_rate_guard as NRG  # noqa: E402


def bucket(**kw):
    return types.SimpleNamespace(**kw)


def build(name):
    if name == "no_buckets":
        return types.SimpleNamespace()
    if name == "all_remaining":
        return types.SimpleNamespace(
            requests_min=bucket(limit=10, remaining=5, reset_seconds=120),
            tokens_hour=bucket(limit=100, remaining=50, reset_seconds=120))
    if name == "zero_limit":
        return types.SimpleNamespace(
            requests_min=bucket(limit=0, remaining=0, reset_seconds=120))
    if name == "exhausted_ok":
        return types.SimpleNamespace(
            requests_hour=bucket(limit=10, remaining=0, reset_seconds=120))
    if name == "exhausted_short_reset":
        return types.SimpleNamespace(
            tokens_min=bucket(limit=10, remaining=0, reset_seconds=30))
    if name == "remaining_seconds_now":
        return types.SimpleNamespace(
            tokens_hour=bucket(limit=5, remaining=0,
                               remaining_seconds_now=90, reset_seconds=30))
    raise ValueError(name)


def main():
    data = sys.stdin.read()
    c_cases = {}
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if "case" in obj:
            c_cases[obj["case"]] = obj

    mism = 0
    for case in c_cases:
        c_ret = 1 if c_cases[case].get("ret") else 0
        py_ret = 1 if NRG._has_exhausted_bucket_in_object(build(case)) else 0
        if c_ret == py_ret:
            print(f"ok [{case}]")
        else:
            mism += 1
            print(f"MISMATCH [{case}]  C={c_ret} PY={py_ret}")
    print(f"\nRESULT: {len(c_cases)-mism}/{len(c_cases)} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
