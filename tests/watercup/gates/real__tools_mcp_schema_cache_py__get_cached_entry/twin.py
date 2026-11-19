"""twin for tools/mcp_schema_cache.py:get_cached_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_schema_cache.py"
    PY_FN = "get_cached_entry"
    C_FN = "get_cached_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
