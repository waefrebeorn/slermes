"""twin for cron/lifecycle_guard.py:_lifecycle_command_scan_with_data_exemption (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_lifecycle_command_scan_with_data_exemption"
    C_FN = "cron_lifecycle_lifecycle_command_scan_with_data_exemption"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
