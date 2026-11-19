"""twin for cron/scheduler_provider.py:_existing_profile_homes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler_provider.py"
    PY_FN = "_existing_profile_homes"
    C_FN = "_existing_profile_homes"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
