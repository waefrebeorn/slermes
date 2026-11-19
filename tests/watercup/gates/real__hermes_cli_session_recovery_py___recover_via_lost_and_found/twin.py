"""twin for hermes_cli/session_recovery.py:_recover_via_lost_and_found (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_recovery.py"
    PY_FN = "_recover_via_lost_and_found"
    C_FN = "_recover_via_lost_and_found"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
