"""twin for hermes_cli/session_recovery.py:_lost_and_found_plausibility_errors (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "_lost_and_found_plausibility_errors"
    C_FN = "_lost_and_found_plausibility_errors"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
