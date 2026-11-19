"""twin for cron/delivery_queue.py:_transaction (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/delivery_queue.py"
    PY_FN = "_transaction"
    C_FN = "_transaction"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
