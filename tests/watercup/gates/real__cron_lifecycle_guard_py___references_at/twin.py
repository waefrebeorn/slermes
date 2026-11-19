"""twin for cron/lifecycle_guard.py:_references_at (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_references_at"
    C_FN = "_references_at"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
