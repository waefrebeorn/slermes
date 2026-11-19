"""twin for tools/computer_use/tool.py:stop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/tool.py"
    PY_FN = "stop"
    C_FN = "noop_stop"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
