"""twin for agent/verification_stop.py:build_verify_on_stop_nudge (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_stop.py"
    PY_FN = "build_verify_on_stop_nudge"
    C_FN = "build_verify_on_stop_nudge"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
