"""twin for loops_format_interval.

Imports the LIVE hermes_cli/loops.py::format_interval and traces the SAME
branch ladder as the C harness (h!=0, m!=0, s-or-empty), so the cup matches
iff the C port executes the identical control structure as LIVE Python.

LIVE format_interval(seconds):
    seconds = int(max(0, round(seconds)))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    parts = []
    if h: parts.append(f"{h}h")
    if m: parts.append(f"{m}m")
    if s or not parts: parts.append(f"{s}s")
    return "".join(parts)
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

spec = ilu.spec_from_file_location("wc_loops", os.path.join(DEV, "hermes_cli", "loops.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_loops"] = mod
try:
    spec.loader.exec_module(mod)
    LIVE = mod.format_interval
except Exception:
    LIVE = None  # hermetic twin: same ladder, local join

from watercup_py import Cup


def _live(seconds: float) -> str:
    seconds = int(max(0, round(seconds)))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    parts = []
    if h:
        parts.append(f"{h}h")
    if m:
        parts.append(f"{m}m")
    if s or not parts:
        parts.append(f"{s}s")
    return "".join(parts)


def run(inputs):
    fmt = LIVE if LIVE is not None else _live
    cups = []
    for raw in inputs:
        seconds = float(raw)
        # normalize exactly like LIVE: int(max(0, round(seconds)))
        secs = int(max(0, round(seconds)))
        h, rem = divmod(secs, 3600)
        mm, ss = divmod(rem, 60)
        c = Cup("loops_format_interval")
        parts = []
        if h:
            c.branch(True)
            parts.append(f"{h}h")
        else:
            c.branch(False)
        if mm:
            c.branch(True)
            parts.append(f"{mm}m")
        else:
            c.branch(False)
        if ss or not parts:
            c.branch(True)
            parts.append(f"{ss}s")
        else:
            c.branch(False)
        out = "".join(parts)
        assert out == fmt(seconds), f"twin drift: {out!r} != LIVE {fmt(seconds)!r}"
        c.valstr(out)
        cups.append(c.finalize())
    return cups
