"""twin for tools/cronjob_tools.py:_notify_provider_jobs_changed_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/cronjob_tools.py"
    PY_FN = "_notify_provider_jobs_changed_safe"
    C_FN = "cronjob_notify_provider_jobs_changed_safe"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
