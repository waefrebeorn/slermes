"""twin for tools/code_execution_tool.py:check_sandbox_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_execution_tool.py"
    PY_FN = "check_sandbox_requirements"
    C_FN = "check_sandbox_requirements"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
