"""twin for cron/jobs.py:_record_timezone_migration_catchup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "_record_timezone_migration_catchup"
    C_FN = "_record_timezone_migration_catchup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
