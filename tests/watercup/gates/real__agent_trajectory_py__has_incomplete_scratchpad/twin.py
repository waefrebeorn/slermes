"""twin for agent/trajectory.py:has_incomplete_scratchpad (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/trajectory.py"
    PY_FN = "has_incomplete_scratchpad"
    C_FN = "trj_has_incomplete_scratchpad"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
