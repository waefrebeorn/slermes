"""twin for tools/computer_use/permissions.py:_doctor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/permissions.py"
    PY_FN = "_doctor"
    C_FN = "cmd_doctor"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
