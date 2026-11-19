"""twin for cron/scheduler_provider.py:scheduler_for_profile_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler_provider.py"
    PY_FN = "scheduler_for_profile_mode"
    C_FN = "scheduler_for_profile_mode"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
