"""twin for hermes_cli/web_server.py:_run_cron_dashboard_io (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_run_cron_dashboard_io"
    C_FN = "_run_cron_dashboard_io"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
