"""twin for fuzzy_match_detect_escape_drift.

Imports LIVE tools/fuzzy_match.py::_detect_escape_drift and traces the SAME
branch ladder as the C harness: quote-suspect pre-check -> matched-region
build -> per-suspect (in-new AND in-old AND absent-from-region) rungs ->
error classification value.

Inputs are "old_string|new_string|content"; matched region is whole content.
The classification value ("ERR"/"NONE") is computed by the LIVE function.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location("wc_fuzzy_match", os.path.join(DEV, "tools", "fuzzy_match.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_fuzzy_match"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

FN = "_detect_escape_drift"
SQ = "\\'"    # backslash + apostrophe
DQ = '\\"'    # backslash + double-quote


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup(FN)
        parts = raw.split("|")
        old_s = parts[0] if len(parts) > 0 else ""
        new_s = parts[1] if len(parts) > 1 else ""
        content = parts[2] if len(parts) > 2 else ""
        c.valstr(old_s)
        c.valstr(new_s)
        c.valstr(content)

        matches = [(0, len(content))]
        live_res = mod._detect_escape_drift(content, matches, old_s, new_s)
        region = content

        err = False
        has_suspects = SQ in new_s or DQ in new_s
        c.branch(has_suspects)
        if has_suspects:
            c.loop()  # one match -> one region-build tick
            for suspect in (SQ, DQ):
                hit = suspect in new_s and suspect in old_s and suspect not in region
                c.branch(hit)
                if hit:
                    err = True
                    break
        c.branch(err)
        live_err = live_res is not None
        assert err == live_err, "twin ladder diverged from LIVE (%r)" % (raw,)
        c.valstr("ERR" if live_err else "NONE")
        cups.append(c.finalize())
    return cups
