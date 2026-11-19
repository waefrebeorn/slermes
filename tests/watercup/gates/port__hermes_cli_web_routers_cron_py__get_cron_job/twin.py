"""twin for hermes_cli/web_routers/cron.py:get_cron_job (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/cron.py"
    PY_FN = "get_cron_job"
    C_FN = "ws_get_cron_job"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
