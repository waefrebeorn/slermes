"""twin for tools/skills_tool.py:_capture_required_environment_variables (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_tool.py"
    PY_FN = "_capture_required_environment_variables"
    C_FN = "_capture_required_environment_variables"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
