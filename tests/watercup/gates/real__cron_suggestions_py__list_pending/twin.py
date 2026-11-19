"""twin for cron/suggestions.py:list_pending (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/suggestions.py"
    PY_FN = "list_pending"
    C_FN = "list_pending"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
