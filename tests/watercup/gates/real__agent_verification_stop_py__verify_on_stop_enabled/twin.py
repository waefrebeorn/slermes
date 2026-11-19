"""twin for agent/verification_stop.py:verify_on_stop_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_stop.py"
    PY_FN = "verify_on_stop_enabled"
    C_FN = "verify_on_stop_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
