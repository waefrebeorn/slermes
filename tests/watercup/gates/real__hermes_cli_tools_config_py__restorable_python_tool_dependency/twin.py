"""twin for hermes_cli/tools_config.py:restorable_python_tool_dependency (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "restorable_python_tool_dependency"
    C_FN = "tools_config_restorable_python_tool_dependency"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
