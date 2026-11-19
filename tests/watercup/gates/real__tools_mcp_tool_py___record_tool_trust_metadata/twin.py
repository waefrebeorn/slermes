"""twin for tools/mcp_tool.py:_record_tool_trust_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_record_tool_trust_metadata"
    C_FN = "mcp_record_tool_trust_metadata"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
