"""twin for cron/lifecycle_guard.py:_read_script_for_scanning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_read_script_for_scanning"
    C_FN = "_read_script_for_scanning"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
