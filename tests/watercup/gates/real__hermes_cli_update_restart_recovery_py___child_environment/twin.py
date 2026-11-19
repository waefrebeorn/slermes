"""twin for hermes_cli/update_restart_recovery.py:_child_environment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_restart_recovery.py"
    PY_FN = "_child_environment"
    C_FN = "_child_environment"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
