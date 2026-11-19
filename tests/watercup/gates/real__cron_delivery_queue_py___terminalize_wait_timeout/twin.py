"""twin for cron/delivery_queue.py:_terminalize_wait_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/delivery_queue.py"
    PY_FN = "_terminalize_wait_timeout"
    C_FN = "_terminalize_wait_timeout"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
