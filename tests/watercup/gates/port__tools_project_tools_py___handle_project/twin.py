"""twin for tools/project_tools.py:_handle_project (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/project_tools.py"
    PY_FN = "_handle_project"
    C_FN = "_handle_project"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
