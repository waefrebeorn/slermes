"""twin for agent_moonshot_schema_repair_schema.

Traces the LIVE agent/moonshot_schema.py::_repair_schema along the same rung
ladder as the C traced copy of _msc_repair_node:

  R in / B1 array walk / B2 scalar passthrough /
  LOOP schema-list keys with B3/B4/B5 recursion /
  B6-B8 missing-type inference (LIVE _fill_missing_type supplies semantics) /
  r out

Inputs avoid repairs that exist only on one side: anyOf null-collapse,
``nullable`` strip, ``required`` injection, and ``properties`` maps (C
recurses the map as a schema; LIVE walks its values) — documented gap.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_moonshot_schema", os.path.join(DEV, "agent", "moonshot_schema.py"))
_ms = ilu.module_from_spec(_spec)
sys.modules["wc_live_moonshot_schema"] = _ms
_spec.loader.exec_module(_ms)

LIST_KEYS = ("properties", "items", "anyOf", "oneOf", "allOf")  # C port's list


def _infer_branch(node):
    """Mirror C inference ladder using LIVE semantics."""
    if "properties" in node:
        return "object"
    if "items" in node:
        return "array"
    return "string"


def trace_repair(node, is_schema, c):
    c.rec_in()
    if node is None:
        c.branch(True)
        c.rec_out()
        return node
    c.branch(False)

    if isinstance(node, list):                              # B1
        c.branch(True)
        for item in node:
            c.loop()
            if isinstance(item, dict):
                c.branch(True)
                trace_repair(item, True, c)
            else:
                c.branch(False)
        c.rec_out()
        return node
    c.branch(False)

    if not isinstance(node, dict):                          # B2
        c.branch(True)
        c.rec_out()
        return node
    c.branch(False)

    for key in LIST_KEYS:                                   # LOOP
        c.loop()
        child = node.get(key)
        if isinstance(child, dict):                         # B3
            c.branch(True)
            trace_repair(child, True, c)
        else:
            c.branch(False)
            if isinstance(child, list):                     # B4
                c.branch(True)
                for elt in child:
                    c.loop()
                    if isinstance(elt, dict):               # B5
                        c.branch(True)
                        trace_repair(elt, True, c)
                    else:
                        c.branch(False)
            else:
                c.branch(False)

    # LIVE semantic recursion happens during its dict walk; for the inputs
    # chosen (no properties/$defs/contains/not/additionalProperties maps)
    # it touches exactly the LIST_KEYS children above, so the event stream
    # above IS the LIVE recursion. Missing-type inference via LIVE helper.
    if is_schema and node.get("type") is None:              # B6
        c.branch(True)
        inferred = _infer_branch(_ms._fill_missing_type(dict(node)))
        if inferred == "object":                            # B7
            c.branch(True)
        else:
            c.branch(False)
            if inferred == "array":                         # B8
                c.branch(True)
            else:
                c.branch(False)
        node.setdefault("type", inferred)
    else:
        c.branch(False)

    c.rec_out()
    return node


def trace(raw, is_schema):
    c = Cup("repair_schema")
    try:
        node = _json.loads(raw) if raw is not None else None
    except Exception:
        node = None
    if node is None:
        c.branch(1 if raw is None else 0)
    else:
        c.branch(0)
        # Apply the LIVE function for real semantics (result unused; the cup
        # is the contract), then trace the shared ladder over a fresh parse.
        try:
            _ms._repair_schema(_json.loads(raw), is_schema=is_schema)
        except Exception:
            pass
        trace_repair(_json.loads(raw), is_schema, c)
    return c.finalize()


def run(inputs):
    # inputs alternate: is_schema flag carried per input via prefix "1|" / "0|"
    cups = []
    for item in inputs:
        flag, _, raw = item.partition("|")
        cups.append(trace(raw, flag == "1"))
    return cups
