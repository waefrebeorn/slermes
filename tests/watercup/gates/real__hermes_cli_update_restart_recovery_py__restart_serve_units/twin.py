"""twin for hermes_cli/update_restart_recovery.py:restart_serve_units (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_restart_recovery.py"
    PY_FN = "restart_serve_units"
    C_FN = "restart_serve_units"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
