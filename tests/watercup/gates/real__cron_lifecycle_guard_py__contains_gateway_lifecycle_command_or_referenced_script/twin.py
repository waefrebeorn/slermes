"""twin for cron/lifecycle_guard.py:contains_gateway_lifecycle_command_or_referenced_script (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "contains_gateway_lifecycle_command_or_referenced_script"
    C_FN = "cron_lifecycle_contains_gateway_lifecycle_command_or_referenced_script"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
