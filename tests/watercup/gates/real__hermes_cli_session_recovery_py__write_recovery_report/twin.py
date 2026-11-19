"""twin for hermes_cli/session_recovery.py:write_recovery_report (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "write_recovery_report"
    C_FN = "write_recovery_report"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
