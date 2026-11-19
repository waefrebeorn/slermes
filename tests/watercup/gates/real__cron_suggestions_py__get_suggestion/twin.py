"""twin for cron/suggestions.py:get_suggestion (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/suggestions.py"
    PY_FN = "get_suggestion"
    C_FN = "get_suggestion"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
