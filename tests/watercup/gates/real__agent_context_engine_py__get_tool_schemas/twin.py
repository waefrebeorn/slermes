"""twin for agent/context_engine.py:get_tool_schemas (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_engine.py"
    PY_FN = "get_tool_schemas"
    C_FN = "get_tool_schemas"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
