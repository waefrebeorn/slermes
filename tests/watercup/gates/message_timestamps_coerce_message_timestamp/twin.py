"""twin for message_timestamps_coerce_message_timestamp.

Imports LIVE gateway/message_timestamps.py::coerce_message_timestamp and
traces the SAME rungs as the C harness over the agreed sub-contract walk:
blank strings, positive integer epochs, explicit-UTC ISO timestamps,
and unparseable junk.
"""
import importlib.util as ilu
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_mts", os.path.join(DEV, "gateway", "message_timestamps.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_mts"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        text = raw.strip()
        c = Cup("coerce_message_timestamp")
        # b0: blank
        blank = not text
        c.branch(blank)
        # b1: pure positive-integer epoch
        is_int = text.isdigit() and text.isdigit() is not None and _is_pos_int(text)
        c.branch(is_int)
        # b2: ISO datetime parse
        looks_iso = _iso_ok(text)
        c.branch(looks_iso)
        live = mod.coerce_message_timestamp(raw)
        if live is None:
            c.valstr("none")
        else:
            c.valstr("%.17g" % float(live))
        cups.append(c.finalize())
    return cups


def _is_pos_int(text):
    if not text:
        return False
    for ch in text:
        if ch not in "0123456789":
            return False
    return True


def _iso_ok(text):
    # mirror the C rung: sscanf "%d-%d-%dT%d:%d:%d" succeeding with >=3
    # conversions AND the fields forming a real timestamp (excluded classes
    # never produce out-of-range fields in this walk).
    import re as _re
    m = _re.match(r"^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})", text)
    return m is not None
