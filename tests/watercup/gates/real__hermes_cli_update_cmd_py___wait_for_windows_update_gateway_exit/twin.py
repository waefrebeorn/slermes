"""twin for hermes_cli/update_cmd.py:_wait_for_windows_update_gateway_exit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_wait_for_windows_update_gateway_exit"
    C_FN = "_wait_for_windows_update_gateway_exit"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
