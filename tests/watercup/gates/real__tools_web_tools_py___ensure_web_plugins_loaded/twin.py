"""twin for tools/web_tools.py:_ensure_web_plugins_loaded (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_tools.py"
    PY_FN = "_ensure_web_plugins_loaded"
    C_FN = "web_ensure_web_plugins_loaded"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
