"""twin for gateway/platforms/api_server.py:_handle_browser_control_ws (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_handle_browser_control_ws"
    C_FN = "_handle_browser_control_ws"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
