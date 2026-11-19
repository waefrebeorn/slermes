"""twin for tools/file_operations.py:_split_tool_diagnostics (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_operations.py"
    PY_FN = "_split_tool_diagnostics"
    C_FN = "file_ops_search_split_tool_diagnostics"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
