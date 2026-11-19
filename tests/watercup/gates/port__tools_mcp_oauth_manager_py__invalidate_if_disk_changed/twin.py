"""twin for tools/mcp_oauth_manager.py:invalidate_if_disk_changed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth_manager.py"
    PY_FN = "invalidate_if_disk_changed"
    C_FN = "1"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
