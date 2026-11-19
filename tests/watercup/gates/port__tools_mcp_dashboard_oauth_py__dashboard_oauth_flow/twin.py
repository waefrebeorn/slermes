"""twin for tools/mcp_dashboard_oauth.py:dashboard_oauth_flow (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_dashboard_oauth.py"
    PY_FN = "dashboard_oauth_flow"
    C_FN = "tools_mcp_dashboard_oauth_dashboard_oauth_flow"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
