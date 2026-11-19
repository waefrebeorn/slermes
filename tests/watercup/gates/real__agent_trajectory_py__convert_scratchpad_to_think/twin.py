"""twin for agent/trajectory.py:convert_scratchpad_to_think (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/trajectory.py"
    PY_FN = "convert_scratchpad_to_think"
    C_FN = "convert_scratchpad_to_think"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
