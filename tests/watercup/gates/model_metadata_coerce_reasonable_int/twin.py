"""twin for model_metadata_coerce_reasonable_int.

Traces LIVE agent/model_metadata.py::_coerce_reasonable_int(value) with the
same rung order as the C harness (src/agent/provider_metadata.c::
coerce_reasonable_int, called with LIVE's defaults minimum=1024,
maximum=10_000_000):
  B1 null / LOOP lead space-tab / B2 empty / LOOP comma removal /
  B3 trailing garbage / B4 out-of-range.
ASCII inputs only — Python str.strip() also strips unicode whitespace and
newlines where the C port only handles spaces/tabs.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_model_metadata", os.path.join(DEV, "agent", "model_metadata.py"))
_mm = ilu.module_from_spec(_spec)
sys.modules["wc_live_model_metadata"] = _mm
_spec.loader.exec_module(_mm)

MINIMUM, MAXIMUM = 1024, 10_000_000


def trace(raw):
    c = Cup("coerce_reasonable_int")
    value = None if raw == "NULL" else raw
    if value is None:                                # B1
        c.branch(True)
        res = -1
    else:
        c.branch(False)
        pos = 0
        n = len(value)
        while pos < n and value[pos] in " \t":       # LOOP lead ws
            c.loop()
            pos += 1
        stripped = value[pos:]
        c.branch(stripped == "")                     # B2
        if stripped == "":
            res = -1
        else:
            kept = []
            for ch in stripped:                      # LOOP comma removal
                c.loop()
                if ch != ",":
                    kept.append(ch)
            buf = "".join(kept)
            body = buf.lstrip(" \t")
            core = body.rstrip(" \t")
            # endptr_ok mirrors C: digits then optional trailing space/tab run
            endptr_ok = False
            s = body
            k = 0
            seen_digit = False
            while k < len(s) and (s[k].isdigit() or (not seen_digit and s[k] in "+-")):
                if s[k].isdigit():
                    seen_digit = True
                k += 1
                if not seen_digit and k >= len(s):
                    break
            if seen_digit:
                rest = s[k:]
                endptr_ok = rest == "" or all(ch in " \t" for ch in rest)
            c.branch(not endptr_ok)                  # B3
            if not endptr_ok:
                res = -1
            else:
                val = int(core)
                in_range = MINIMUM <= val <= MAXIMUM
                c.branch(not in_range)               # B4
                res = val if in_range else -1
    live = _mm._coerce_reasonable_int(value)
    expected = None if res == -1 else res
    assert live == expected, (raw, live, expected)
    return expected, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        _, cup = trace(raw)
        cups.append(cup)
    return cups
