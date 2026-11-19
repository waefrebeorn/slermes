"""twin for tools/browser_tool.py:_auto_local_for_private_urls (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_auto_local_for_private_urls"
    C_FN = "browser_auto_local_for_private_urls"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
