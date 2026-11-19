"""twin for cron/scheduler_provider.py:_misfire_grace_minutes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler_provider.py"
    PY_FN = "_misfire_grace_minutes"
    C_FN = "_misfire_grace_minutes"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
