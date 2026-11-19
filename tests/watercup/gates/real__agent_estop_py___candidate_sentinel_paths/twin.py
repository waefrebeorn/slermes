"""twin for agent/estop.py:_candidate_sentinel_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/estop.py"
    PY_FN = "_candidate_sentinel_paths"
    C_FN = "_candidate_sentinel_paths"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
