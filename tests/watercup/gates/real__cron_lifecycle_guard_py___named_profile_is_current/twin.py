"""twin for cron/lifecycle_guard.py:_named_profile_is_current (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_named_profile_is_current"
    C_FN = "cron_lifecycle_named_profile_is_current"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
