"""twin for agent/verification_stop.py:_candidate_cwds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verification_stop.py"
    PY_FN = "_candidate_cwds"
    C_FN = "_candidate_cwds"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
