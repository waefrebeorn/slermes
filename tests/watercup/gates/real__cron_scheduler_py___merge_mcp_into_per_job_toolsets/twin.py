"""twin for cron/scheduler.py:_merge_mcp_into_per_job_toolsets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_merge_mcp_into_per_job_toolsets"
    C_FN = "_merge_mcp_into_per_job_toolsets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
