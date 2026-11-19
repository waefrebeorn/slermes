"""twin for cron/scheduler.py:_maybe_mirror_cron_delivery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_maybe_mirror_cron_delivery"
    C_FN = "_maybe_mirror_cron_delivery"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
