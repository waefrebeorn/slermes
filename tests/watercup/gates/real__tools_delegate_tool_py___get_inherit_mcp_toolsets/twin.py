"""twin for tools/delegate_tool.py:_get_inherit_mcp_toolsets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_get_inherit_mcp_toolsets"
    C_FN = "delegate_get_inherit_mcp_toolsets"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
