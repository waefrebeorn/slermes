"""twin for cron/scheduler.py:_windows_cron_bootstrap_argv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_windows_cron_bootstrap_argv"
    C_FN = "_windows_cron_bootstrap_argv"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
