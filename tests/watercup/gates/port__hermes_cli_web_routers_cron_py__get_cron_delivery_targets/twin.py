"""twin for hermes_cli/web_routers/cron.py:get_cron_delivery_targets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/cron.py"
    PY_FN = "get_cron_delivery_targets"
    C_FN = "get_cron_delivery_targets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
