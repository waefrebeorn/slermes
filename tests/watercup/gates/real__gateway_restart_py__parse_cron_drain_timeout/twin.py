"""twin for gateway/restart.py:parse_cron_drain_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart.py"
    PY_FN = "parse_cron_drain_timeout"
    C_FN = "parse_cron_drain_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
