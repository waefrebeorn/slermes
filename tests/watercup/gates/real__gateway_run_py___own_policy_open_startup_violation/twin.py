"""twin for gateway/run.py:_own_policy_open_startup_violation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_own_policy_open_startup_violation"
    C_FN = "_own_policy_open_startup_violation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
