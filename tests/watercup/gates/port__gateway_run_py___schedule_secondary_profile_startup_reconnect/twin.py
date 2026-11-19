"""twin for gateway/run.py:_schedule_secondary_profile_startup_reconnect (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_schedule_secondary_profile_startup_reconnect"
    C_FN = "_schedule_secondary_profile_startup_reconnect"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
