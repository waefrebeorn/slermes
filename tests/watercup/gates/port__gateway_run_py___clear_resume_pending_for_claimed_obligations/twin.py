"""twin for gateway/run.py:_clear_resume_pending_for_claimed_obligations (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_clear_resume_pending_for_claimed_obligations"
    C_FN = "_clear_resume_pending_for_claimed_obligations"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
