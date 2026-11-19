"""twin for tools/mcp_tool.py:_strip_reserved_meta_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_strip_reserved_meta_keys"
    C_FN = "_strip_reserved_meta_keys"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
