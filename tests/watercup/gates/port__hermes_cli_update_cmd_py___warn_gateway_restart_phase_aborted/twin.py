"""twin for hermes_cli/update_cmd.py:_warn_gateway_restart_phase_aborted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_warn_gateway_restart_phase_aborted"
    C_FN = "_warn_gateway_restart_phase_aborted"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
