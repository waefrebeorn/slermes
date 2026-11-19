"""twin for hermes_cli/gateway_windows.py:_build_gateway_argv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway_windows.py"
    PY_FN = "_build_gateway_argv"
    C_FN = "gw_u_build_gateway_argv"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
