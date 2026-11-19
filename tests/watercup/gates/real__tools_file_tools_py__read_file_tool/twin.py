"""twin for tools/file_tools.py:read_file_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_tools.py"
    PY_FN = "read_file_tool"
    C_FN = "read_file_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
