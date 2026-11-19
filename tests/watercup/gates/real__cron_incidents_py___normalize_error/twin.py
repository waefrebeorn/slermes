"""twin for cron/incidents.py:_normalize_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/incidents.py"
    PY_FN = "_normalize_error"
    C_FN = "_normalize_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
