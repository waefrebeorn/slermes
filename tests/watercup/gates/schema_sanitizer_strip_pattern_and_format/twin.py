"""twin for schema_sanitizer_strip_pattern_and_format.

Imports the LIVE Python tools/schema_sanitizer.py::strip_pattern_and_format
and traces the SAME branch ladder as the C harness, so the cup matches iff the
C port executes the identical control structure as LIVE Python. Also asserts
the traced stripped-count equals the LIVE function's real result (anti-drift).
"""
import os, sys, json, copy
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "tools"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_schema_sanitizer", os.path.join(DEV, "tools", "schema_sanitizer.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_schema_sanitizer"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

STRIP_KEYS = {"pattern", "format"}
SCHEMA_MARKERS = {"type", "anyOf", "oneOf", "allOf"}


def trace_walk(cup, node, stripped):
    cup.rec_in()
    if isinstance(node, dict):
        cup.branch(True)
        is_schema = False
        for k in node.keys():
            cup.loop()
            if k in SCHEMA_MARKERS:
                cup.branch(True)
                is_schema = True
                break
            cup.branch(False)
        cup.branch(is_schema)
        for k in list(node.keys()):
            cup.loop()
            hit = is_schema and k in STRIP_KEYS
            cup.branch(hit)
            if hit:
                stripped[0] += 1
                continue
            trace_walk(cup, node[k], stripped)
    else:
        cup.branch(False)
        if isinstance(node, list):
            cup.branch(True)
            for item in node:
                cup.loop()
                trace_walk(cup, item, stripped)
        else:
            cup.branch(False)
    cup.rec_out()


def run(inputs):
    cups = []
    for raw in inputs:
        tools = json.loads(raw)
        # authoritative LIVE outcome (mutates a copy; we only read the count)
        live_tools, live_stripped = mod.strip_pattern_and_format(copy.deepcopy(tools))
        c = Cup("strip_pattern_and_format")
        stripped = [0]
        if not tools:
            c.branch(True)
        else:
            c.branch(False)
            # C port guard: json_parse failure rung (inputs here always parse)
            c.branch(False)
            if isinstance(tools, list):
                c.branch(True)
                for tool in tools:
                    c.loop()
                    if not isinstance(tool, dict):
                        c.branch(False)
                        continue
                    c.branch(True)
                    fn = tool.get("function")
                    c.branch(isinstance(fn, dict))
                    params = None
                    if isinstance(fn, dict):
                        params = fn.get("parameters")
                        c.branch(isinstance(params, dict))
                    if params is None:
                        params = tool.get("parameters")
                        c.branch(isinstance(params, dict))
                    if isinstance(params, dict):
                        trace_walk(c, params, stripped)
            else:
                c.branch(False)
        c.valstr(str(stripped[0]))
        assert stripped[0] == live_stripped, (
            f"traced count {stripped[0]} != LIVE count {live_stripped}")
        cups.append(c.finalize())
    return cups
