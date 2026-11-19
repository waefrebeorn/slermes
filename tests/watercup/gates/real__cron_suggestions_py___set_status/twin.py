"""twin for cron/suggestions.py:_set_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/suggestions.py"
    PY_FN = "_set_status"
    C_FN = "cron_sugg_set_status"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
