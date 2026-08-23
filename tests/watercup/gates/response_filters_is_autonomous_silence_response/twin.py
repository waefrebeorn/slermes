"""twin for response_filters_is_autonomous_silence_response.

Imports LIVE gateway/response_filters.py and traces the SAME rung ladder as
the C harness (response_filters.c::cli_gateway_response_filters_is_...).
Canonicalization V-events use LIVE _canonical_silence_candidate on line
tokens only (whole-response canonical forms can differ on embedded newlines).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location(
    "wc_live_response_filters", os.path.join(DEV, "gateway", "response_filters.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_response_filters"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

MARKERS = mod.LIVE_GATEWAY_SILENT_MARKERS
canon = mod._canonical_silence_candidate


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("is_autonomous_silence_response")
        response = None if raw == "null" else raw
        result = 0
        if not isinstance(response, str):
            c.branch(True)
        else:
            c.branch(False)
            stripped = response.strip()
            if not stripped:
                c.branch(True)
            else:
                c.branch(False)
                if canon(stripped) in MARKERS:
                    c.branch(True)
                    result = 1
                else:
                    c.branch(False)
                    lines = [ln for ln in stripped.splitlines() if ln.strip()]
                    first = lines[0] if lines else None
                    if first is not None:
                        c.branch(True)
                        c.valstr(canon(first))
                    else:
                        c.branch(False)
                    last = lines[-1] if lines and len(lines) > 1 else None
                    if last is not None:
                        c.branch(True)
                        c.valstr(canon(last))
                    else:
                        c.branch(False)
                    if (first is not None and canon(first) in MARKERS) or (
                            last is not None and canon(last) in MARKERS):
                        c.branch(True)
                        result = 1
                    else:
                        c.branch(False)
                        if stripped.upper().startswith("[SILENT]"):
                            c.branch(True)
                            result = 1
                        else:
                            c.branch(False)
        c.valstr(str(result))
        cups.append(c.finalize())
    return cups
