"""twin for tools/browser_tool.py:_handle_browser_command_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_handle_browser_command_timeout"
    C_FN = "_handle_browser_command_timeout"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
