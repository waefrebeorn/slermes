"""twin for cron/lifecycle_guard.py:charge_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "charge_path"
    C_FN = "charge_path"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
