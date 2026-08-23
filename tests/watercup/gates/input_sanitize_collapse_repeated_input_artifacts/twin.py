"""twin for input_sanitize_collapse_repeated_input_artifacts.

Traces LIVE hermes_cli/input_sanitize.py::collapse_repeated_input_artifacts
with the same rung order as the C harness. min_repeats comes from the first
gate.json entry; remaining entries are texts.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(SLERMES, "tests", "watercup"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_input_sanitize",
    os.path.join(DEV, "hermes_cli", "input_sanitize.py"))
_is = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_input_sanitize"] = _is
_spec.loader.exec_module(_is)

MIN_REPEATS = 4


def trace(text, min_repeats=MIN_REPEATS):
    c = Cup("collapse_repeated_input_artifacts")
    marker = _is._DESKTOP_PASTE_ARTIFACT
    result = text
    if not text:
        c.branch(True)                                   # B1
        c.valstr(text or "")                             # V1
        return c.finalize()
    c.branch(False)
    index = len(text)
    repeat_count = 0
    while index >= len(marker) and text[index - len(marker):index] == marker:
        c.loop()
        c.branch(True)                                   # B2
        repeat_count += 1
        index -= len(marker)
    c.branch(False)                                      # B2 exit
    if repeat_count < min_repeats:
        c.branch(True)                                   # B3
        c.valstr(text)                                   # V1
        return c.finalize()
    c.branch(False)
    start = index
    if start >= 2 and text[start - 2:start] == "[e":
        c.branch(True)                                   # B4
        start -= 2
    else:
        c.branch(False)
        if start >= 1 and text[start - 1] == "[":
            c.branch(True)                               # B5
            start -= 1
        else:
            c.branch(False)
    result = text[:start]
    c.valstr(result)                                     # V1
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]
