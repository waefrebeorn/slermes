"""twin for hermes_cli/web_routers/cron.py:list_cron_blueprints (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/cron.py"
    PY_FN = "list_cron_blueprints"
    C_FN = "list_cron_blueprints"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
