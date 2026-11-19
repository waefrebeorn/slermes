"""twin for tools/browser_tool.py:_maybe_autoinstall_chromium (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_maybe_autoinstall_chromium"
    C_FN = "browser_maybe_autoinstall_chromium"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
