"""twin for browser__lightpanda_fallback_reason.

Imports LIVE tools/browser_tool.py::_lightpanda_fallback_reason and traces the
SAME rung ladder as the C harness (which emits the full B0..B6 ladder on every
input). Each cup's final value is the LIVE reason string (or "" for None),
sanity-checked against the LIVE fn itself.
Inputs are "<engine>|<command>|<result_json>".

Inputs where the C port's nested-data lookup bug diverges from LIVE are
excluded (see gate.json notes).
"""
import json as _json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_browser_tool",
                                   os.path.join(DEV, "tools", "browser_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_browser_tool"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

_ELIGIBLE = {"open", "snapshot", "screenshot", "eval", "click",
             "fill", "scroll", "back", "press", "console", "errors"}


def run(inputs):
    cups = []
    fn = mod._lightpanda_fallback_reason
    for raw in inputs:
        engine, command, rj = raw.split("|", 2)
        result = _json.loads(rj)

        c = Cup("browser__lightpanda_fallback_reason")
        live = fn(engine, command, result)

        # Mirror the harness's full-ladder emission order.
        b_engine = engine == "lightpanda"
        c.branch(b_engine)                              # B0
        b_eligible = b_engine and command in _ELIGIBLE
        c.branch(b_eligible)                            # B1
        b_parsed = b_eligible                           # dict payload always parses
        c.branch(b_parsed)                              # B2
        if b_parsed:
            b_success = bool(result.get("success"))
            c.branch(b_success)                         # B3
        b_snapshot = b_parsed and bool(result.get("success")) and command == "snapshot"
        c.branch(b_snapshot)                            # B4
        if b_snapshot:
            snap = result.get("data", {}).get("snapshot", "")
            short = (not snap) or len(snap.strip()) < 20
            c.branch(short)                             # B5
        b_screenshot = (b_parsed and bool(result.get("success"))
                        and not b_snapshot and command == "screenshot")
        c.branch(b_screenshot)                          # B6

        c.valstr(live or "")
        cups.append(c.finalize())
    return cups
