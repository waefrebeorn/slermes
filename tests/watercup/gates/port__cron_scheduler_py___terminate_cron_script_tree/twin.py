"""twin for cron/scheduler.py:_terminate_cron_script_tree (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_terminate_cron_script_tree"
    C_FN = "_terminate_cron_script_tree"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
