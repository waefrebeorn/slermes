"""twin for hermes_cli/gateway_windows.py:windowless_gateway_restart_spec (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway_windows.py"
    PY_FN = "windowless_gateway_restart_spec"
    C_FN = "gw_windowless_gateway_restart_spec"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
