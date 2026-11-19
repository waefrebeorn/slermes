"""twin for tools/terminal_tool.py:_validate_workdir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_validate_workdir"
    C_FN = "_validate_workdir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
