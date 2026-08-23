"""twin for schema_sanitizer_strip_slash_enum.

Imports the LIVE tools/schema_sanitizer.py::strip_slash_enum and traces the
SAME dispatch ladder as the C harness (which calls the REAL C port):

  per tool (loop tick): B1 tool-is-dict / B2 function.parameters dict /
  B3 top-level parameters dict / V stripped count from the LIVE fn.
"""
import os
import json
import sys
import importlib
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_schema_sanitizer",
    os.path.join(DEV, "tools", "schema_sanitizer.py"))
_ss = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_schema_sanitizer"] = _ss
_spec.loader.exec_module(_ss)


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("strip_slash_enum")
        tools = json.loads(raw)
        if isinstance(tools, list):
            for tool in tools:
                c.loop()
                is_dict = isinstance(tool, dict)
                c.branch(is_dict)                       # B1
                if not is_dict:
                    continue
                fn = tool.get("function")
                if isinstance(fn, dict):
                    params = fn.get("parameters")
                    b2 = isinstance(params, dict)
                    c.branch(b2)                        # B2
                    if not b2:
                        c.branch(isinstance(tool.get("parameters"), dict))  # B3
                else:
                    c.branch(False)                     # B2
                    c.branch(isinstance(tool.get("parameters"), dict))      # B3
        else:
            c.branch(False)
            c.branch(False)
        _, stripped = _ss.strip_slash_enum(json.loads(raw))
        c.valstr(str(stripped))
        cups.append(c.finalize())
    return cups
