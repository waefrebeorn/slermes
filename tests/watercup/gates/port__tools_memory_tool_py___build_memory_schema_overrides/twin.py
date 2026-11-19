"""twin for tools/memory_tool.py:_build_memory_schema_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/memory_tool.py"
    PY_FN = "_build_memory_schema_overrides"
    C_FN = "memory_tool_build_memory_schema_overrides"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
