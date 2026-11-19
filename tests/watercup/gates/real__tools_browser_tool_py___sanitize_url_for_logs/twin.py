"""twin for tools/browser_tool.py:_sanitize_url_for_logs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_sanitize_url_for_logs"
    C_FN = "_sanitize_url_for_logs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
