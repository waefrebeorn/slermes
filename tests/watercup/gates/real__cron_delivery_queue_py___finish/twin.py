"""twin for cron/delivery_queue.py:_finish (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/delivery_queue.py"
    PY_FN = "_finish"
    C_FN = "_finish"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
