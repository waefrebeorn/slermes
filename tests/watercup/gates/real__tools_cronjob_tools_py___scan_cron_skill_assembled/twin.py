"""twin for tools/cronjob_tools.py:_scan_cron_skill_assembled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/cronjob_tools.py"
    PY_FN = "_scan_cron_skill_assembled"
    C_FN = "_scan_cron_skill_assembled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
