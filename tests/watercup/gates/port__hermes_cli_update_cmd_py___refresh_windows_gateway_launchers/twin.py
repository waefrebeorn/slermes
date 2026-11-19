"""twin for hermes_cli/update_cmd.py:_refresh_windows_gateway_launchers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_refresh_windows_gateway_launchers"
    C_FN = "uc_refresh_windows_gateway_launchers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
