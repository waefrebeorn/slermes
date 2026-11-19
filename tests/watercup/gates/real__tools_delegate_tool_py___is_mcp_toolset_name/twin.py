"""twin for tools/delegate_tool.py:_is_mcp_toolset_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_is_mcp_toolset_name"
    C_FN = "delegate_is_mcp_toolset_name"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
