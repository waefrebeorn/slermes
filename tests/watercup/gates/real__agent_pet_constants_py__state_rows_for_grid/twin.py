"""twin for agent/pet/constants.py:state_rows_for_grid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/constants.py"
    PY_FN = "state_rows_for_grid"
    C_FN = "state_rows_for_grid"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
