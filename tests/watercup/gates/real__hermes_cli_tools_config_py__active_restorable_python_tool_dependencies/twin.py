"""twin for hermes_cli/tools_config.py:active_restorable_python_tool_dependencies (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "active_restorable_python_tool_dependencies"
    C_FN = "active_restorable_python_tool_dependencies"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
