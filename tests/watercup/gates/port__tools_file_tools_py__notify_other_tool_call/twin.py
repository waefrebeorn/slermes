"""twin for tools/file_tools.py:notify_other_tool_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "notify_other_tool_call"
    C_FN = "file_tools_notify_other_tool_call"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
