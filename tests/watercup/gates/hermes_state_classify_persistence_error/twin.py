"""twin for hermes_state_classify_persistence_error.

Traces the LIVE hermes_state.py::classify_persistence_error ladder with the
SAME rung order as the C harness (string inputs; typed-exception branches are
always false). The traced bucket is asserted against the LIVE fn's return on
every input.
"""
import importlib.util as ilu
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_hermes_state", os.path.join(DEV, "hermes_state.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_hermes_state"] = mod
spec.loader.exec_module(mod)
live_classify = mod.classify_persistence_error

from watercup_py import Cup

CORRUPTION_MARKERS = (
    "malformed",
    "file is not a database",
    "not a database",
    "database corruption",
)
DISK_FULL_MARKERS = (
    "no space left on device",
    "not enough space",
    "database or disk is full",
    "disk full",
    "full disk",
    "enospc",
)


def trace_one(raw):
    c = Cup("classify_persistence_error")
    c.branch(raw is None)                       # B1
    if raw is None:
        label = "unknown"
        c.valstr(label)
        cup = c.finalize()
        assert live_classify(raw) == label, raw
        return cup
    # B2-B4: isinstance checks for SessionTurnLeaseLostError /
    # CompressionSessionClosedError / CompressionSessionBusyError — false for str
    c.branch(isinstance(raw, Exception))
    c.branch(False)
    c.branch(False)

    text = str(raw).lower()
    b5 = "turn lease" in text
    c.branch(b5)
    if b5:
        label = "turn_lease"
        assert live_classify(raw) == label, raw
        c.valstr(label)
        return c.finalize()

    b6 = "closed by compression" in text
    c.branch(b6)
    if b6:
        label = "compression_closed"
        assert live_classify(raw) == label, raw
        c.valstr(label)
        return c.finalize()

    b7 = "being compressed" in text
    b8 = "compression lease" in text
    c.branch(b7)
    c.branch(b8)
    if b7 or b8:
        label = "compression"
        assert live_classify(raw) == label, raw
        c.valstr(label)
        return c.finalize()

    for marker in CORRUPTION_MARKERS:
        hit = marker in text
        c.branch(hit)
        if hit:
            label = "corrupt"
            assert live_classify(raw) == label, raw
            c.valstr(label)
            return c.finalize()

    b13 = "locked" in text
    b14 = "busy" in text
    c.branch(b13)
    c.branch(b14)
    if b13 or b14:
        label = "locked"
        assert live_classify(raw) == label, raw
        c.valstr(label)
        return c.finalize()

    disk = False
    for marker in DISK_FULL_MARKERS:
        hit = marker in text
        c.branch(hit)
        if hit:
            disk = True
            break
    if disk:
        c.branch(True)
        c.branch(True)
        c.branch(True)
    else:
        b21 = "disk" in text
        c.branch(b21)
        b22 = (not b21) and ("readonly" in text)
        c.branch(b22)
        b23 = (not b21) and (not b22) and ("read-only" in text)
        c.branch(b23)
        disk = b21 or b22 or b23
    label = "disk" if disk else "unknown"
    assert live_classify(raw) == label, raw
    c.valstr(label)
    return c.finalize()


def run(inputs):
    return [trace_one(x) for x in inputs]


if __name__ == "__main__":
    inputs = __import__("json").load(open(os.path.join(HERE, "gate.json")))["inputs"]
    print("\n".join(run(inputs)))
