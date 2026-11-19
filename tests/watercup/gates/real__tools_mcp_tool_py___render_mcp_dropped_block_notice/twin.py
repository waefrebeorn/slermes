"""twin for tools/mcp_tool.py:_render_mcp_dropped_block_notice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_render_mcp_dropped_block_notice"
    C_FN = "_render_mcp_dropped_block_notice"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
