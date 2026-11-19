"""twin for tools/memory_tool.py:reset_consolidation_failures (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/memory_tool.py"
    PY_FN = "reset_consolidation_failures"
    C_FN = "memory_tool_pure_store_reset_consolidation_failures"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
