"""twin for cron/lifecycle_guard.py:_contains_launchctl_gateway_lifecycle (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_contains_launchctl_gateway_lifecycle"
    C_FN = "cron_lifecycle_contains_launchctl_gateway_lifecycle"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
