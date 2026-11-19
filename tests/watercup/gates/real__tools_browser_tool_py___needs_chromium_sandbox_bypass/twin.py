"""twin for tools/browser_tool.py:_needs_chromium_sandbox_bypass (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_needs_chromium_sandbox_bypass"
    C_FN = "browser_needs_chromium_sandbox_bypass"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
