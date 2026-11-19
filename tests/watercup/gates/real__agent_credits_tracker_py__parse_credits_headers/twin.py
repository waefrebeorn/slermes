"""twin for agent/credits_tracker.py:parse_credits_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credits_tracker.py"
    PY_FN = "parse_credits_headers"
    C_FN = "parse_credits_headers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
