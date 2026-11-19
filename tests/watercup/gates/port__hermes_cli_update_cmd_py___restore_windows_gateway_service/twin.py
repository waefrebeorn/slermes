"""twin for hermes_cli/update_cmd.py:_restore_windows_gateway_service (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_restore_windows_gateway_service"
    C_FN = "_restore_windows_gateway_service"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
