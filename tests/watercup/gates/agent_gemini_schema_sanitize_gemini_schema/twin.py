"""twin for agent_gemini_schema_sanitize_gemini_schema.

Traces the LIVE agent/gemini_schema.py::sanitize_gemini_schema with the same
rung order as the C harness (see harness.c header for the ladder). Leaf
decisions use the LIVE module's _GEMINI_SCHEMA_ALLOWED_KEYS; after tracing,
the twin asserts its traced result equals the LIVE function's actual output —
any drift raises, so a stale twin can never silently pass.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_gemini_schema", os.path.join(DEV, "agent", "gemini_schema.py"))
_gs = ilu.module_from_spec(_spec)
sys.modules["wc_live_gemini_schema"] = _gs
_spec.loader.exec_module(_gs)

ALLOWED = _gs._GEMINI_SCHEMA_ALLOWED_KEYS


def _trace_node(schema, c):
    """Trace one schema node (dict expected on agreed inputs)."""
    c.rec_in()
    cleaned = {}
    for key, value in schema.items():               # LOOP keys
        c.loop()
        if key not in ALLOWED:                      # B2
            c.branch(False)
            continue
        c.branch(True)
        if key == "properties":                     # B3
            c.branch(True)
            if isinstance(value, dict):             # B4
                c.branch(True)
                props = {}
                for prop_name, prop_schema in value.items():   # LOOP props
                    c.loop()
                    c.rec_in()
                    props[prop_name] = _trace_node(prop_schema, c)
                    c.rec_out()
                cleaned[key] = props
            else:
                c.branch(False)
        else:
            c.branch(False)
        if key == "items":                          # B5
            c.branch(True)
            if isinstance(value, dict):             # B6
                c.branch(True)
                c.rec_in()
                cleaned[key] = _trace_node(value, c)
                c.rec_out()
            else:
                c.branch(False)
        else:
            c.branch(False)
        if key == "anyOf":                          # B7
            c.branch(True)
            if isinstance(value, list):             # B8
                c.branch(True)
                out_items = []
                for item in value:                  # LOOP items
                    c.loop()
                    if isinstance(item, dict):      # B9
                        c.branch(True)
                        c.rec_in()
                        out_items.append(_trace_node(item, c))
                        c.rec_out()
                    else:
                        c.branch(False)
                cleaned[key] = out_items
            else:
                c.branch(False)
        else:
            c.branch(False)
        if key not in ("properties", "items", "anyOf"):
            cleaned[key] = value          # plain scalar/leaf copy — no branch

    import math
    enum_val = cleaned.get("enum")                  # B10 list?
    c.branch(isinstance(enum_val, list))
    typed = (isinstance(enum_val, list)
             and cleaned.get("type") in {"integer", "number", "boolean"})  # B11
    c.branch(typed)
    if typed:
        has_nonstr = any(not isinstance(x, str) for x in enum_val)  # B12
        c.branch(has_nonstr)
        if has_nonstr:
            cleaned.pop("enum", None)

    required_val = cleaned.get("required")          # B13 list?
    c.branch(isinstance(required_val, list))
    if isinstance(required_val, list):
        props_val = cleaned.get("properties")
        props_is_dict = isinstance(props_val, dict)  # B14
        c.branch(props_is_dict)
        prop_names = set(props_val.keys()) if props_is_dict else set()
        valid_required = []
        for name in required_val:                    # LOOP required
            c.loop()
            ok = isinstance(name, str) and name in prop_names   # B15
            c.branch(ok)
            if ok:
                valid_required.append(name)
        c.branch(not valid_required)                 # B16
        c.branch(len(valid_required) != len(required_val))       # B17

    c.rec_out()
    return cleaned


def trace(schema_json):
    import json as _json
    c = Cup("sanitize_gemini_schema")
    try:
        schema = _json.loads(schema_json) if schema_json else None
    except Exception:
        schema = None
    is_obj = isinstance(schema, dict)               # B1
    c.branch(is_obj)
    if not is_obj:
        return None, c.finalize()
    traced = _trace_node(schema, c)
    # honesty check: traced ladder must agree with LIVE output
    live = _gs.sanitize_gemini_schema(schema)
    if traced != live:
        raise AssertionError("traced ladder diverges from LIVE sanitize_gemini_schema for %r" % schema)
    return live, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        _, cup = trace(s)
        cups.append(cup)
    return cups
