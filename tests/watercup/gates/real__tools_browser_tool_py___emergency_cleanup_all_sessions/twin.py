"""twin for tools/browser_tool.py:_emergency_cleanup_all_sessions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_emergency_cleanup_all_sessions"
    C_FN = "browser__emergency_cleanup_all_sessions"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
