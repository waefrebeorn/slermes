"""twin for gateway/systemd_notify.py:watchdog_interval_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/systemd_notify.py"
    PY_FN = "watchdog_interval_seconds"
    C_FN = "sd_n_watchdog_interval_seconds"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
