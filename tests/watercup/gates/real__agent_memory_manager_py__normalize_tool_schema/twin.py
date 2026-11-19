"""twin for agent/memory_manager.py:normalize_tool_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_manager.py"
    PY_FN = "normalize_tool_schema"
    C_FN = "normalize_tool_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
