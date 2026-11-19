"""twin for agent/replay_cleanup.py:is_dangerous_confirmation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/replay_cleanup.py"
    PY_FN = "is_dangerous_confirmation"
    C_FN = "is_dangerous_confirmation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
