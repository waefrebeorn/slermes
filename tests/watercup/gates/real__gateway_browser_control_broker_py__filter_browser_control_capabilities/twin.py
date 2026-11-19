"""twin for gateway/browser_control_broker.py:filter_browser_control_capabilities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/browser_control_broker.py"
    PY_FN = "filter_browser_control_capabilities"
    C_FN = "filter_browser_control_capabilities"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
