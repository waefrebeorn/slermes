"""twin for cron/scheduler_provider.py:claim_fire (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler_provider.py"
    PY_FN = "claim_fire"
    C_FN = "claim_fire"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
