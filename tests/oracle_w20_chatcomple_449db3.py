#!/usr/bin/env python3
"""Differential oracle for agent/chat_completion_helpers_relay.py (w20_chatcomple_449db3).

The live RelayChatAccumulator (plus the flatten/glue helpers it imports) is
loaded from the quarry and replayed chunk by chunk over the same fixture, then
the finalize() dict is compared with the C port's.
"""
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


# psutil's C extension is refused in this sandbox (ImportError: DLL load failed
# while importing _psutil_windows: Access is denied); the live relay code only
# touches it for process metrics, so a permissive stub keeps the real logic live.
class _PsutilDummy(object):
    def __init__(self, *a, **k):
        pass

    def __call__(self, *a, **k):
        return _PsutilDummy()

    def __getattr__(self, name):
        return _PsutilDummy()

    def __iter__(self):
        return iter(())

    def __float__(self):
        return 0.0

    def __int__(self):
        return 0

    def __bool__(self):
        return False


if "psutil" not in sys.modules:
    _ps = types.ModuleType("psutil")
    _ps.__getattr__ = lambda name: _PsutilDummy
    sys.modules["psutil"] = _ps

NAME = "w20_chatcomple_449db3_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "chat_completion_helpers_relay.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "run":
        acc = live.RelayChatAccumulator()
        chunks = r.get("chunks")
        for chunk in chunks if isinstance(chunks, list) else []:
            acc.observe(chunk)
        return acc.finalize()
    if f == "flatten":
        return live.flatten_message_text(r.get("content"), sep="")
    if f == "separate":
        return live.separate_glued_reasoning_blocks(r.get("previous") or "", r.get("delta"))
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
print("ORACLE w20_chatcomple_449db3: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
