"""twin for tools/memory_tool.py:get_builtin_memory_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/memory_tool.py"
    PY_FN = "get_builtin_memory_config"
    C_FN = "get_builtin_memory_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
