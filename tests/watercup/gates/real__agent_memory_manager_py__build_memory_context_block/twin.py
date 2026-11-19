"""twin for agent/memory_manager.py:build_memory_context_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_manager.py"
    PY_FN = "build_memory_context_block"
    C_FN = "build_memory_context_block"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
