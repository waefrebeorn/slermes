"""twin for gateway/run.py:_clear_planned_restart_notification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_clear_planned_restart_notification"
    C_FN = "gw_clear_planned_restart_notification"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
