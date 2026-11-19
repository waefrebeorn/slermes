"""twin for tools/mcp_dashboard_oauth.py:mark_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_dashboard_oauth.py"
    PY_FN = "mark_error"
    C_FN = "tools_mcp_dashboard_oauth_mark_error"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
