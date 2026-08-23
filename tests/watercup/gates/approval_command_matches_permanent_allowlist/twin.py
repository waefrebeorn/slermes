"""twin for approval_command_matches_permanent_allowlist.

Imports LIVE tools/approval.py::_command_matches_permanent_allowlist, seeds its
permanent-approval set from the allowlist JSON in each input, and traces the
SAME rungs as the C harness: B(command non-empty), per-pattern
B(exists)/B(matched), then V of the LIVE verdict.
"""
import os, sys, json as _json
from fnmatch import fnmatchcase
HERE = os.path.dirname(os.path.abspath(__file__))
DEV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))))
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_approval", os.path.join(DEV, "tools", "approval.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_approval"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for inp in inputs:
        command, _, json_txt = inp.rpartition("|")
        try:
            patterns = [p for p in _json.loads(json_txt)]
        except Exception:
            patterns = []
        c = Cup("command_matches_permanent_allowlist")
        if not command:
            c.branch(True)
            result = False
        else:
            c.branch(False)
            result = False
            for i in range(len(patterns) + 1):
                if i >= len(patterns):
                    c.branch(False)
                    break
                c.branch(True)
                pat = patterns[i]
                if "*" in pat:
                    match = fnmatchcase(command, pat)
                else:
                    match = (command == pat)
                c.branch(match)
                if match:
                    result = True
                    break
        # seed LIVE state and take the LIVE verdict as V
        with mod._lock:
            if isinstance(mod._permanent_approved, set):
                mod._permanent_approved = {p for p in patterns}
            else:
                mod._permanent_approved.clear()
                mod._permanent_approved.extend(patterns)
        live = mod._command_matches_permanent_allowlist(command)
        c.valstr("True" if live else "False")
        if bool(live) != result:
            pass  # divergence surfaces via the V event / final cup mismatch
        cups.append(c.finalize())
    return cups
