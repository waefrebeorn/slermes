"""twin for hermes_cli/update_cmd.py:_warn_pending_fleet_restart_on_startup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_warn_pending_fleet_restart_on_startup"
    C_FN = "_warn_pending_fleet_restart_on_startup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
