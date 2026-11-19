"""twin for hermes_cli/update_cmd.py:_write_fleet_restart_pending_marker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_write_fleet_restart_pending_marker"
    C_FN = "_write_fleet_restart_pending_marker"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
