"""twin for tools/mcp_tool.py:_reconnect_or_reraise_group (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_reconnect_or_reraise_group"
    C_FN = "mcp_reconnect_or_reraise_group"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
