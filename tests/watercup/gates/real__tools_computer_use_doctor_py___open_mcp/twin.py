"""twin for tools/computer_use/doctor.py:_open_mcp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/doctor.py"
    PY_FN = "_open_mcp"
    C_FN = "_open_mcp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
