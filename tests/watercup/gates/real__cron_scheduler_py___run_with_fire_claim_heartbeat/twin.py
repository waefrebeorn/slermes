"""twin for cron/scheduler.py:_run_with_fire_claim_heartbeat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_run_with_fire_claim_heartbeat"
    C_FN = "_run_with_fire_claim_heartbeat"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
