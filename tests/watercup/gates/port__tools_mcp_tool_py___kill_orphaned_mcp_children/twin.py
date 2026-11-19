"""twin for tools/mcp_tool.py:_kill_orphaned_mcp_children (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_kill_orphaned_mcp_children"
    C_FN = "mct_kill_orphaned_mcp_children"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
