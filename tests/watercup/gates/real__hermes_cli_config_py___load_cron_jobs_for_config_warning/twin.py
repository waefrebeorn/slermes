"""twin for hermes_cli/config.py:_load_cron_jobs_for_config_warning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_load_cron_jobs_for_config_warning"
    C_FN = "_load_cron_jobs_for_config_warning"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
