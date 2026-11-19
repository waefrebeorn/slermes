"""twin for tools/browser_tool.py:_using_lightpanda_engine (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_using_lightpanda_engine"
    C_FN = "browser_using_lightpanda_engine"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
