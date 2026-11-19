"""twin for cron/jobs.py:get_timezone_migration_catchup_stats (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "get_timezone_migration_catchup_stats"
    C_FN = "get_timezone_migration_catchup_stats"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
