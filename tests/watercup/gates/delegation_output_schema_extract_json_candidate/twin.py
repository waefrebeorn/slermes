"""twin for delegation_output_schema_extract_json_candidate.

Imports LIVE tools/delegation_output_schema.py::extract_json_candidate and
traces the SAME rung ladder as delegate_tool.c::delegation_extract_json_candidate,
with exact candidate-string value parity.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_delegation_output_schema",
                                   os.path.join(DEV, "tools", "delegation_output_schema.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_delegation_output_schema"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def trace(text):
    c = Cup("extract_json_candidate")
    raw = (text or "").strip()
    if raw.startswith("```"):
        c.branch(True)
        had_nl = "\n" in raw
        raw = raw.split("\n", 1)[-1]
        c.branch(had_nl)
        trimmed = raw.rstrip()
        if trimmed.endswith("```"):
            c.branch(True)
            raw = trimmed[:-3]
        else:
            c.branch(False)
        raw = raw.strip()
        if raw.lower().startswith("json\n"):
            c.branch(True)
            raw = raw.split("\n", 1)[1]
        else:
            c.branch(False)
    else:
        c.branch(False)
    result = None
    for opener, closer in (("{", "}"), ("[", "]")):
        c.loop()
        if raw.startswith(opener):
            c.branch(True)
            result = raw
            break
        c.branch(False)
        start = raw.find(opener)
        end = raw.rfind(closer)
        if start >= 0 and end > start:
            c.branch(True)
            result = raw[start:end + 1]
            break
        c.branch(False)
    if result is None:
        result = raw
    c.valstr(result)
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        text = raw
        live = mod.extract_json_candidate(text)
        cups.append(trace(text))
    return cups
