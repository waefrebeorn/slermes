"""twin for tools/code_execution_tool.py:_uses_hermes_python_environment (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_execution_tool.py"
    PY_FN = "_uses_hermes_python_environment"
    C_FN = "_uses_hermes_python_environment"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
