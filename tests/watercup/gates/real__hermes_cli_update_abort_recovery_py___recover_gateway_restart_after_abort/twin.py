"""twin for hermes_cli/update_abort_recovery.py:_recover_gateway_restart_after_abort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_abort_recovery.py"
    PY_FN = "_recover_gateway_restart_after_abort"
    C_FN = "_recover_gateway_restart_after_abort"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
