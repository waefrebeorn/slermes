"""twin for hermes_cli/gateway.py:_windows_gateway_should_absorb_console_controls (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_windows_gateway_should_absorb_console_controls"
    C_FN = "cgw_u_windows_gateway_should_absorb_console_controls"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
