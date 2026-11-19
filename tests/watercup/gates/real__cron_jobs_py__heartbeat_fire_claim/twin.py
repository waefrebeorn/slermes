"""twin for cron/jobs.py:heartbeat_fire_claim (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "heartbeat_fire_claim"
    C_FN = "cronjobs_heartbeat_fire_claim"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
