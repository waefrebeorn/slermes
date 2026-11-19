"""twin for agent/curator.py:apply_automatic_transitions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator.py"
    PY_FN = "apply_automatic_transitions"
    C_FN = "apply_automatic_transitions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
