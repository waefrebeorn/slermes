"""twin for tools/code_execution_tool.py:execute_code (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_execution_tool.py"
    PY_FN = "execute_code"
    C_FN = "execute_code"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
