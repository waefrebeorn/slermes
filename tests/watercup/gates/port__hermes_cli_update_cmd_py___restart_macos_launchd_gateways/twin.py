"""twin for hermes_cli/update_cmd.py:_restart_macos_launchd_gateways (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_restart_macos_launchd_gateways"
    C_FN = "_restart_macos_launchd_gateways"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
