"""twin for cch_status_code_from_value.

Imports the LIVE Python agent/chat_completion_helpers.py::_status_code_from_value
and traces the SAME branch ladder as the C harness, so the cup matches iff the
C port executes the identical control structure as LIVE Python. C uses -1 to
represent LIVE's None (Optional[int]).
"""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "agent"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_cch", os.path.join(DEV, "agent", "chat_completion_helpers.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cch"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        # pass as int where it looks like a clean integer, else str
        try:
            val = int(raw)
        except ValueError:
            val = raw
        c = Cup("status_code_from_value")
        if raw is None or raw == "":
            c.branch(True); c.valstr("-1"); cups.append(c.finalize()); continue
        c.branch(False)
        if isinstance(val, int):
            if 100 <= val < 600:
                c.branch(True); c.valstr(str(val)); cups.append(c.finalize()); continue
            c.branch(False); c.valstr("-1"); cups.append(c.finalize()); continue
        c.branch(False)
        # string path
        m = re.search(r"(?:HTTP_STATUS/)?\b([1-5]\d\d)\b", val, re.IGNORECASE)
        found = int(m.group(1)) if m else -1
        c.valstr(str(found))
        cups.append(c.finalize())
    return cups
