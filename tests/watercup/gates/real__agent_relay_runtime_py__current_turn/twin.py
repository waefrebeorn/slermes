"""twin for agent/relay_runtime.py:current_turn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "current_turn"
    C_FN = "current_turn"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
