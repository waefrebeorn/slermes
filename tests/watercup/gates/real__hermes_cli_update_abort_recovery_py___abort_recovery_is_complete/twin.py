"""twin for hermes_cli/update_abort_recovery.py:_abort_recovery_is_complete (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_abort_recovery.py"
    PY_FN = "_abort_recovery_is_complete"
    C_FN = "_abort_recovery_is_complete"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
