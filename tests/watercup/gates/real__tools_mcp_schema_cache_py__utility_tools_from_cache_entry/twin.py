"""twin for tools/mcp_schema_cache.py:utility_tools_from_cache_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_schema_cache.py"
    PY_FN = "utility_tools_from_cache_entry"
    C_FN = "utility_tools_from_cache_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
