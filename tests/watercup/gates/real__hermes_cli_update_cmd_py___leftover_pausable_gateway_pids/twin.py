"""twin for hermes_cli/update_cmd.py:_leftover_pausable_gateway_pids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_leftover_pausable_gateway_pids"
    C_FN = "_leftover_pausable_gateway_pids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
