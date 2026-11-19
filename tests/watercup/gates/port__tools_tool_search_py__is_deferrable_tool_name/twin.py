"""twin for tools/tool_search.py:is_deferrable_tool_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_search.py"
    PY_FN = "is_deferrable_tool_name"
    C_FN = "ts_is_deferrable_tool_name"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
