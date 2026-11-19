"""twin for cron/lifecycle_guard.py:_iter_shell_command_payloads (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_iter_shell_command_payloads"
    C_FN = "_iter_shell_command_payloads"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
