#!/usr/bin/env python3
"""Differential oracle for agent/context_compressor_typed_input.py
(gate w20_contextcom_971c8d).

The live module is a pure leaf (json + typing only), so it is imported
directly from the quarry and each harness line replayed through the live
helper with the same inputs. The two callables the module expects arrive as
fixture data: `real_user` as flags 1:1 with turns, `render_args` as a
raw->rendered map (absent -> identity).
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

NAME = "w20_contextcom_971c8d_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "context_compressor_typed_input.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "typed_turns_pointer":
        return live.TYPED_TURNS_POINTER
    if f == "call_arguments":
        ramap = r.get("render_args") if isinstance(r.get("render_args"), dict) else {}
        raw = r.get("raw") if isinstance(r.get("raw"), str) else ""
        return live._call_arguments(ramap.get(raw, raw))
    if f == "typed_summary_messages":
        turns = r.get("turns") if isinstance(r.get("turns"), list) else []
        records = r.get("records") if isinstance(r.get("records"), list) else []
        slices = r.get("slices") if isinstance(r.get("slices"), list) else []
        flags = r.get("real_user") if isinstance(r.get("real_user"), list) else []
        ramap = r.get("render_args") if isinstance(r.get("render_args"), dict) else {}
        owner = {id(t): bool(flags[i]) for i, t in enumerate(turns) if i < len(flags)}
        return live.typed_summary_messages(
            turns, records, [(s[0], s[1]) for s in slices],
            lambda m: owner.get(id(m), False),
            lambda s: ramap.get(s, s))
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
    except Exception as exc:  # noqa: BLE001 - report a live failure verbatim
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_contextcom_971c8d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
