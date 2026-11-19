"""twin for gateway/platforms/api_server.py:_inject_browser_control_artifacts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_inject_browser_control_artifacts"
    C_FN = "_inject_browser_control_artifacts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
