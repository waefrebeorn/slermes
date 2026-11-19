"""twin for agent/fast_mode.py:begin_turn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/fast_mode.py"
    PY_FN = "begin_turn"
    C_FN = "begin_turn"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
