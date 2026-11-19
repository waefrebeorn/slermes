"""twin for hermes_cli/gateway_windows.py:_force_terminate_known_gateway_pids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway_windows.py"
    PY_FN = "_force_terminate_known_gateway_pids"
    C_FN = "gw_u_force_terminate_known_gateway_pids"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
