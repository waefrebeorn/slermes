"""twin for tools/browser_extension_router.py:routed_browser_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_extension_router.py"
    PY_FN = "routed_browser_handler"
    C_FN = "bcr_routed_browser_handler"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
