"""twin for cron/jobs.py:_heartbeat_fire_claim_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_heartbeat_fire_claim_locked"
    C_FN = "cronjobs_heartbeat_fire_claim_locked"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
