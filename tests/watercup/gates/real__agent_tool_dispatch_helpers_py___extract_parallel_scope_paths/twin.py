"""twin for agent/tool_dispatch_helpers.py:_extract_parallel_scope_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_dispatch_helpers.py"
    PY_FN = "_extract_parallel_scope_paths"
    C_FN = "_extract_parallel_scope_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
