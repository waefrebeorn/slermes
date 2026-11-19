"""twin for hermes_cli/update_cmd.py:_run_pending_fleet_restart (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_run_pending_fleet_restart"
    C_FN = "_run_pending_fleet_restart"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
