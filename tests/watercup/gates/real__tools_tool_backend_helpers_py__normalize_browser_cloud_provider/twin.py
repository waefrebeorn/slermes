"""twin for tools/tool_backend_helpers.py:normalize_browser_cloud_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_backend_helpers.py"
    PY_FN = "normalize_browser_cloud_provider"
    C_FN = "tools_tool_backend_helpers_normalize_browser_cloud_provider"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
