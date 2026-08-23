"""twin for moonshot_fill_missing_type.

Imports LIVE agent/moonshot_schema.py::_fill_missing_type and traces the SAME
ladder as the C harness over its substring port. Inputs are restricted to the
agreed sub-contract (see gate.json notes).
"""
import importlib.util as ilu
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_moonshot", os.path.join(DEV, "agent", "moonshot_schema.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_moonshot"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw_s in inputs:
        node = json.loads(raw_s)
        c = Cup("fill_missing_type")
        node_type = node.get("type")
        has_type = "type" in node and node_type not in {None, ""}
        c.branch(bool("type" in node))
        if has_type:
            c.valstr("unchanged")
            cups.append(c.finalize())
            continue
        if "properties" in node or "required" in node or "additionalProperties" in node:
            c.branch(True)
            c.valstr("object")
        else:
            c.branch(False)
            if "items" in node or "prefixItems" in node:
                c.branch(True)
                c.valstr("array")
            else:
                c.branch(False)
                enum = node.get("enum") if "enum" in node else None
                # Mirror the C substring rung: any "enum" key takes this rung.
                # Only string-sample enums (and the empty list, where LIVE
                # falls back to "string" too) are in the agreed walk.
                is_enum = isinstance(enum, list)
                c.branch(is_enum)
                if is_enum:
                    c.valstr(mod._fill_missing_type(node)["type"] if enum else "string")
                else:
                    c.valstr("string")
        cups.append(c.finalize())
    return cups
