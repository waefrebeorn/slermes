"""twin for hermes_cli/update_cmd.py:_restart_systemd_gateway_units_best_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_restart_systemd_gateway_units_best_effort"
    C_FN = "_restart_systemd_gateway_units_best_effort"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
