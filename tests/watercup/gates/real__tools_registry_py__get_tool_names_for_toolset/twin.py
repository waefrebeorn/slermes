"""twin for tools/registry.py:get_tool_names_for_toolset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/registry.py"
    PY_FN = "get_tool_names_for_toolset"
    C_FN = "get_tool_names_for_toolset"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
