"""twin for tools/memory_tool.py:_read_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/memory_tool.py"
    PY_FN = "_read_file"
    C_FN = "_read_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
