"""twin for cron/delivery_queue.py:_prune_terminal_unlocked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/delivery_queue.py"
    PY_FN = "_prune_terminal_unlocked"
    C_FN = "_prune_terminal_unlocked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
