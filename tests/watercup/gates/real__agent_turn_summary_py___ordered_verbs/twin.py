"""twin for agent/turn_summary.py:_ordered_verbs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"
    PY_FN = "_ordered_verbs"
    C_FN = "_ordered_verbs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
