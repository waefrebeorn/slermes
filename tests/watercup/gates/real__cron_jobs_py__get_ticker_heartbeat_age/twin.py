"""twin for cron/jobs.py:get_ticker_heartbeat_age (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "get_ticker_heartbeat_age"
    C_FN = "cronjobs_ticker_heartbeat_age"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
