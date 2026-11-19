"""twin for cron/lifecycle_guard.py:contains_launchctl_submit_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "contains_launchctl_submit_command"
    C_FN = "cron_lifecycle_contains_launchctl_submit_command"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
