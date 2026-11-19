"""twin for hermes_cli/_early_recovery.py:_run_repair_install (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_early_recovery.py"
    PY_FN = "_run_repair_install"
    C_FN = "hermes_cli__early_recovery_u_run_repair_install"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
