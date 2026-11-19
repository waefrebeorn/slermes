"""twin for agent/reactions.py:detect_reaction (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/reactions.py"
    PY_FN = "detect_reaction"
    C_FN = "detect_reaction"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
