"""twin for hermes_cli/web_server.py:_list_cron_job_runs_sync (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_list_cron_job_runs_sync"
    C_FN = "_list_cron_job_runs_sync"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
