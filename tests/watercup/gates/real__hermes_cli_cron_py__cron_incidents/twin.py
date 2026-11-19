"""twin for hermes_cli/cron.py:cron_incidents (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cron.py"
    PY_FN = "cron_incidents"
    C_FN = "cron_incidents"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
