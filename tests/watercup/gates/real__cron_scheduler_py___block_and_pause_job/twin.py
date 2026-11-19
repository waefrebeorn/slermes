"""twin for cron/scheduler.py:_block_and_pause_job (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_block_and_pause_job"
    C_FN = "_block_and_pause_job"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
