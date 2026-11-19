"""twin for agent/credits_tracker.py:_validate_usd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credits_tracker.py"
    PY_FN = "_validate_usd"
    C_FN = "crt_validate_usd"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
